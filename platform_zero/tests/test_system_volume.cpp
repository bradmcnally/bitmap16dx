#include "system_volume.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
std::string read(const std::filesystem::path& path) {
  std::ifstream file(path);
  return std::string(std::istreambuf_iterator<char>(file), {});
}
}

int main() {
  using bitmap16::Zero::adjustSystemVolume;
  const auto root = std::filesystem::temp_directory_path() /
      ("bitmap16-system-volume " + std::to_string(getpid()));
  std::filesystem::create_directories(root);
  const char* path = std::getenv("PATH");
  const std::string originalPath = path == nullptr ? "" : path;
  const auto helper = root / "wpctl";
  {
    std::ofstream script(helper);
    script << R"SH(#!/bin/sh
state="$BITMAP16_VOLUME_TEST_STATE"
case "$1" in
  get-volume)
    [ "$#" = 2 ] && [ "$2" = '@DEFAULT_AUDIO_SINK@' ] || exit 4
    [ ! -f "$state/get-fail" ] || exit 5
    /bin/cat "$state/current"
    ;;
  set-volume)
    [ "$#" = 5 ] && [ "$2" = '-l' ] && [ "$3" = '1.0' ] &&
      [ "$4" = '@DEFAULT_AUDIO_SINK@' ] || exit 6
    [ ! -f "$state/set-fail" ] || exit 7
    printf '%s' "$5" > "$state/last-set"
    percent=${5%\%}
    printf 'Volume: %d.%02d\n' "$((percent / 100))" "$((percent % 100))" > "$state/current"
    ;;
  *) exit 8 ;;
esac
)SH";
  }
  std::filesystem::permissions(helper, std::filesystem::perms::owner_all);
  setenv("PATH", (root.string() + ":" + originalPath).c_str(), 1);
  setenv("BITMAP16_VOLUME_TEST_STATE", root.c_str(), 1);
  std::ofstream(root / "current") << "Volume: 0.50\n";
  require(adjustSystemVolume(true) == 55 && read(root / "last-set") == "55%",
          "Volume up changes the default system sink by five percent with a 100 percent limit");
  require(adjustSystemVolume(false) == 50 && read(root / "last-set") == "50%",
          "Volume down reads the updated shared level");
  std::ofstream(root / "current") << "Volume: 0.80\n";
  require(adjustSystemVolume(false) == 75, "Changes made in HOME are observed rather than overwritten from an app setting");
  std::ofstream(root / "current") << "Volume: 0.00\n";
  require(adjustSystemVolume(false) == 0, "Volume cannot fall below zero");
  std::ofstream(root / "current") << "Volume: 1.00\n";
  require(adjustSystemVolume(true) == 100, "Volume cannot exceed 100 percent");
  const auto lastSet = read(root / "last-set");
  for (const char* invalid : {"unavailable", "Volume: nan", "Volume: -0.2", "Volume: 1.2"}) {
    std::ofstream(root / "current") << invalid << '\n';
    require(adjustSystemVolume(true) == -1 && read(root / "last-set") == lastSet,
            "Invalid system output never changes volume");
  }
  std::ofstream(root / "current") << "Volume: 0.50\n";
  std::ofstream(root / "get-fail");
  require(adjustSystemVolume(true) == -1, "A failed volume query is reported without terminating the app");
  std::filesystem::remove(root / "get-fail");
  std::ofstream(root / "set-fail");
  require(adjustSystemVolume(true) == -1, "A failed system-volume update is reported");
  std::filesystem::remove(root / "set-fail");
  setenv("PATH", "/bitmap16-no-volume-helper", 1);
  require(adjustSystemVolume(true) == -1, "A missing wpctl leaves the app usable");
  setenv("PATH", originalPath.c_str(), 1);
  unsetenv("BITMAP16_VOLUME_TEST_STATE");
  std::filesystem::remove_all(root);
}
