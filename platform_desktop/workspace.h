#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "core/editor.h"
#include "core/settings.h"

namespace bitmap16 {
namespace Desktop {

class Workspace {
 public:
  struct UserPalette {
    std::array<uint16_t, 16> colors = {};
    uint8_t size = 0;
    std::string name;
  };

  bool initialize(Editor& editor);

  const std::filesystem::path& root() const { return root_; }
  const Settings& settings() const { return settings_; }
  Settings& settings() { return settings_; }
  const std::vector<Sketch>& sketches() const { return sketches_; }
  const std::vector<UserPalette>& userPalettes() const {
    return userPalettes_;
  }
  int activeIndex() const { return activeIndex_; }

  bool newSketch(Editor& editor);
  bool openSketch(std::size_t index, Editor& editor);
  bool saveSketch(const Editor& editor, bool saveAsNew);
  bool deleteSketch(std::size_t index, Editor& editor);
  bool undoDelete(Editor& editor);
  bool exportSketch(
      const Sketch& sketch,
      bool scaled,
      std::filesystem::path& outputPath) const;
  bool reloadUserPalettes();
  bool saveSettings() const;

 private:
  bool loadSettings();
  bool loadSketches();
  bool writeSketch(const std::filesystem::path& path, const Sketch& sketch);

  std::filesystem::path root_;
  std::vector<std::filesystem::path> sketchPaths_;
  std::vector<Sketch> sketches_;
  std::vector<UserPalette> userPalettes_;
  std::filesystem::path deletedOriginalPath_;
  std::filesystem::path deletedTrashPath_;
  Settings settings_;
  int activeIndex_ = -1;
};

}  // namespace Desktop
}  // namespace bitmap16
