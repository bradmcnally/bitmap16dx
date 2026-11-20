#include "workspace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "core/palette.h"
#include "core/sketch_codec.h"
#include "palettes.h"
#include "png_export.h"
#include "steam_screenshot.h"

namespace bitmap16 {
namespace Desktop {
namespace {

std::filesystem::path dataRoot() {
  const char* overrideRoot = std::getenv("BITMAP16_DATA_DIR");
  if (overrideRoot != nullptr && overrideRoot[0] != '\0') {
    return overrideRoot;
  }
  const char* xdgData = std::getenv("XDG_DATA_HOME");
  if (xdgData != nullptr && xdgData[0] != '\0') {
    return std::filesystem::path(xdgData) / "bitmap16dx";
  }
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') {
    return std::filesystem::path(home) / ".local" / "share" / "bitmap16dx";
  }
  return std::filesystem::temp_directory_path() / "bitmap16dx";
}

std::filesystem::path settingsRoot(const std::filesystem::path& data) {
#ifdef BITMAP16_CARDPUTER_ZERO
  const char* overrideConfig = std::getenv("BITMAP16_CONFIG_DIR");
  if (overrideConfig != nullptr && overrideConfig[0] != '\0') return overrideConfig;
  // Keep isolated test/data workspaces self-contained unless explicitly overridden.
  const char* overrideData = std::getenv("BITMAP16_DATA_DIR");
  if (overrideData != nullptr && overrideData[0] != '\0') return data;
  const char* config = std::getenv("XDG_CONFIG_HOME");
  if (config != nullptr && config[0] != '\0') return std::filesystem::path(config) / "bitmap16dx";
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] != '\0') return std::filesystem::path(home) / ".config" / "bitmap16dx";
#endif
  return data;
}

std::filesystem::path exportRoot(const std::filesystem::path& data) {
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  // An isolated test workspace keeps its exports with the test data.
  const char* overrideRoot = std::getenv("BITMAP16_DATA_DIR");
  if (overrideRoot == nullptr || overrideRoot[0] == '\0') {
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0') {
      return std::filesystem::path(home) / "Pictures" / "BitMap16DX";
    }
  }
#endif
  return data / "exports";
}

// Version 4 files may include trailing battery-display and LCD-warmth settings.
std::array<uint8_t, 18> encodeSettings(const Settings& settings) {
  return {
      'B', '1', '6', 'S', 4,
      static_cast<uint8_t>(settings.theme),
      settings.defaultGridSize,
      settings.matrixUnits,
      settings.matrixRotation,
      static_cast<uint8_t>(settings.exportFormat),
      static_cast<uint8_t>(settings.shakeUndoEnabled ? 1 : 0),
      settings.displayBrightness,
      settings.matrixBrightness,
      static_cast<uint8_t>(settings.matrixEnabled ? 1 : 0),
      static_cast<uint8_t>(settings.saveWarnings ? 1 : 0),
      static_cast<uint8_t>(settings.cursorStyle),
      static_cast<uint8_t>(settings.batteryDisplay ? 1 : 0),
      settings.displayWarmth,
  };
}

bool decodeSettings(
    const std::vector<uint8_t>& data,
    Settings& settings) {
  if (data.size() < 13 ||
      data[0] != 'B' || data[1] != '1' || data[2] != '6' ||
      data[3] != 'S' ||
      (data[4] < 1 || data[4] > 4)) {
    return false;
  }
  settings.theme = static_cast<ThemeId>(data[5]);
  settings.defaultGridSize = data[6];
  settings.matrixUnits = data[7];
  settings.matrixRotation = data[8];
  settings.exportFormat = static_cast<ExportFormat>(data[9]);
  settings.shakeUndoEnabled = data[10] != 0;
  settings.displayBrightness = data[11];
  settings.matrixBrightness = data[12];
  settings.matrixEnabled = data[4] >= 2 && data.size() >= 14
      ? data[13] != 0
      : false;
  settings.saveWarnings = data[4] >= 3 && data.size() >= 15
      ? data[14] != 0
      : true;
  settings.cursorStyle = data[4] >= 4 && data.size() >= 16
      ? static_cast<CursorStyle>(data[15])
      : CursorStyle::Arrow;
  settings.batteryDisplay = data[4] < 4 || data.size() < 17 || data[16] != 0;
  settings.displayWarmth = data[4] >= 4 && data.size() >= 18 ? data[17] : 0;
  settings = normalizeSettings(settings);
  return true;
}

uint64_t sketchSequence(const std::filesystem::path& path) {
  const std::string stem = path.stem().string();
  const std::size_t separator = stem.find_last_of("_-");
  if (separator == std::string::npos || separator + 1 >= stem.size()) {
    return 0;
  }
  uint64_t sequence = 0;
  for (std::size_t index = separator + 1; index < stem.size(); ++index) {
    const unsigned char character =
        static_cast<unsigned char>(stem[index]);
    if (!std::isdigit(character)) {
      return 0;
    }
    sequence = sequence * 10u +
        static_cast<uint64_t>(character - '0');
  }
  return sequence;
}

}  // namespace

bool Workspace::initialize(Editor& editor) {
  root_ = dataRoot();
  configRoot_ = settingsRoot(root_);
  std::error_code error;
  std::filesystem::create_directories(root_ / "sketches", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "exports", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "palettes", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "trash", error);
  if (error) return false;
  std::filesystem::create_directories(configRoot_, error);
  if (error) return false;
  loadSettings();
  reloadUserPalettes();
  loadSketches();
  // Match the device lifecycle: launch into a fresh unsaved sketch while
  // keeping every saved sketch available in the sketches view.
  newSketch(editor);
  return true;
}

bool Workspace::loadSettings() {
  settings_ = normalizeSettings(Settings{});
  std::ifstream input(configRoot_ / "settings.bin", std::ios::binary);
  if (!input) return false;
  const std::vector<uint8_t> data(
      (std::istreambuf_iterator<char>(input)),
      std::istreambuf_iterator<char>());
  return !input.bad() && decodeSettings(data, settings_);
}

bool Workspace::saveSettings() const {
  const auto data = encodeSettings(settings_);
  std::ofstream output(
      configRoot_ / "settings.bin", std::ios::binary | std::ios::trunc);
  output.write(
      reinterpret_cast<const char*>(data.data()),
      static_cast<std::streamsize>(data.size()));
  return output.good();
}

bool Workspace::loadSketches() {
  sketchPaths_.clear();
  sketches_.clear();
  std::vector<std::filesystem::path> candidates;
  std::error_code error;
  for (const auto& entry :
       std::filesystem::directory_iterator(root_ / "sketches", error)) {
    if (error) return false;
    if (entry.is_regular_file() && entry.path().extension() == ".dat") {
      candidates.push_back(entry.path());
    }
  }
  std::sort(
      candidates.begin(),
      candidates.end(),
      [](const std::filesystem::path& left,
         const std::filesystem::path& right) {
        const uint64_t leftSequence = sketchSequence(left);
        const uint64_t rightSequence = sketchSequence(right);
        if (leftSequence != rightSequence) {
          return leftSequence > rightSequence;
        }
        return left.filename() > right.filename();
      });
  for (const auto& path : candidates) {
    std::ifstream input(path, std::ios::binary);
    const std::vector<uint8_t> data(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    Sketch sketch;
    if (!input.bad() &&
        SketchCodec::decode(data.data(), data.size(), sketch) ==
            SketchCodec::Result::Ok) {
      sketchPaths_.push_back(path);
      sketches_.push_back(sketch);
    }
  }
  return true;
}

bool Workspace::newSketch(Editor& editor) {
  Sketch sketch;
  initializeSketch(
      sketch,
      settings_.defaultGridSize,
      PALETTE_SWEETIE16,
      16);
  editor.reset(sketch);
  activeIndex_ = -1;
  return true;
}

bool Workspace::openSketch(std::size_t index, Editor& editor) {
  if (index >= sketches_.size()) return false;
  activeIndex_ = static_cast<int>(index);
  editor.reset(sketches_[index]);
  return true;
}

bool Workspace::writeSketch(
    const std::filesystem::path& path,
    const Sketch& sketch) {
  std::array<uint8_t, SketchCodec::kCurrentFileSize> data = {};
  std::size_t bytesWritten = 0;
  if (SketchCodec::encode(
          sketch, data.data(), data.size(), bytesWritten) !=
      SketchCodec::Result::Ok) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(
      reinterpret_cast<const char*>(data.data()),
      static_cast<std::streamsize>(bytesWritten));
  return output.good();
}

bool Workspace::saveSketch(const Editor& editor, bool saveAsNew) {
  std::filesystem::path path;
  uint64_t next = 1;
  for (const auto& existing : sketchPaths_) {
    next = std::max(next, sketchSequence(existing) + 1);
  }
  const auto nextPath = [&]() {
    std::filesystem::path candidate;
    do {
      candidate = root_ / "sketches" /
          ("sketch_" + std::to_string(next++) + ".dat");
    } while (std::filesystem::exists(candidate));
    return candidate;
  };
  if (!saveAsNew && activeIndex_ >= 0 &&
      activeIndex_ < static_cast<int>(sketchPaths_.size())) {
    path = sketchPaths_[activeIndex_];
    if (!writeSketch(path, editor.sketch())) return false;
    const std::filesystem::path promoted = nextPath();
    std::error_code error;
    std::filesystem::rename(path, promoted, error);
    if (error) return false;
    path = promoted;
  } else {
    path = nextPath();
    if (!writeSketch(path, editor.sketch())) return false;
  }
  loadSketches();
  const auto found = std::find(sketchPaths_.begin(), sketchPaths_.end(), path);
  activeIndex_ = found == sketchPaths_.end()
      ? -1
      : static_cast<int>(found - sketchPaths_.begin());
  return activeIndex_ >= 0;
}

bool Workspace::duplicateSketch(std::size_t index) {
  if (index >= sketchPaths_.size()) return false;
  const std::filesystem::path activePath =
      activeIndex_ >= 0 &&
              activeIndex_ < static_cast<int>(sketchPaths_.size())
          ? sketchPaths_[activeIndex_]
          : std::filesystem::path{};
  uint64_t next = 1;
  for (const auto& existing : sketchPaths_) {
    next = std::max(next, sketchSequence(existing) + 1);
  }
  std::filesystem::path duplicatePath;
  do {
    duplicatePath = root_ / "sketches" /
        ("sketch_" + std::to_string(next++) + ".dat");
  } while (std::filesystem::exists(duplicatePath));
  if (!writeSketch(duplicatePath, sketches_[index]) || !loadSketches()) {
    return false;
  }
  const auto active =
      std::find(sketchPaths_.begin(), sketchPaths_.end(), activePath);
  activeIndex_ = active == sketchPaths_.end()
      ? -1
      : static_cast<int>(active - sketchPaths_.begin());
  return true;
}

bool Workspace::deleteSketch(std::size_t index) {
  if (index >= sketchPaths_.size()) return false;
  const std::filesystem::path activePath =
      activeIndex_ >= 0 &&
              activeIndex_ < static_cast<int>(sketchPaths_.size())
          ? sketchPaths_[activeIndex_]
          : std::filesystem::path{};
  const std::filesystem::path original = sketchPaths_[index];
  std::filesystem::path trash = root_ / "trash" / original.filename();
  int suffix = 1;
  while (std::filesystem::exists(trash)) {
    trash = root_ / "trash" /
        (original.stem().string() + "-" + std::to_string(suffix++) +
         original.extension().string());
  }
  std::error_code error;
  std::filesystem::rename(original, trash, error);
  if (error) return false;
  deletedOriginalPath_ = original;
  deletedTrashPath_ = trash;
  deletedWasActive_ = original == activePath;
  if (!loadSketches()) return false;
  const auto active =
      std::find(sketchPaths_.begin(), sketchPaths_.end(), activePath);
  activeIndex_ = active == sketchPaths_.end()
      ? -1
      : static_cast<int>(active - sketchPaths_.begin());
  return true;
}

bool Workspace::undoDelete() {
  if (deletedOriginalPath_.empty() || deletedTrashPath_.empty() ||
      !std::filesystem::exists(deletedTrashPath_)) {
    return false;
  }
  const std::filesystem::path activePath =
      activeIndex_ >= 0 &&
              activeIndex_ < static_cast<int>(sketchPaths_.size())
          ? sketchPaths_[activeIndex_]
          : std::filesystem::path{};
  std::filesystem::path restored = deletedOriginalPath_;
  int suffix = 1;
  while (std::filesystem::exists(restored)) {
    restored = deletedOriginalPath_.parent_path() /
        (deletedOriginalPath_.stem().string() + "-restored-" +
         std::to_string(suffix++) +
         deletedOriginalPath_.extension().string());
  }
  std::error_code error;
  std::filesystem::rename(deletedTrashPath_, restored, error);
  if (error) return false;
  const bool restoreActive = deletedWasActive_;
  deletedOriginalPath_.clear();
  deletedTrashPath_.clear();
  deletedWasActive_ = false;
  if (!loadSketches()) return false;
  const std::filesystem::path pathToActivate =
      restoreActive ? restored : activePath;
  const auto active =
      std::find(sketchPaths_.begin(), sketchPaths_.end(), pathToActivate);
  activeIndex_ = active == sketchPaths_.end()
      ? -1
      : static_cast<int>(active - sketchPaths_.begin());
  return restoreActive ? activeIndex_ >= 0 : true;
}

bool Workspace::exportSketch(
    const Sketch& sketch,
    bool scaled,
    std::filesystem::path& outputPath) const {
  const std::filesystem::path directory = exportRoot(root_);
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) return false;
  int next = 1;
  do {
    std::ostringstream name;
    name << "export-" << std::setw(4) << std::setfill('0') << next++
         << ".png";
    outputPath = directory / name.str();
  } while (std::filesystem::exists(outputPath));
  return PngExport::write(
      outputPath, sketch, scaled, settings_.exportFormat);
}

bool Workspace::exportSteamScreenshot(
    const Sketch& sketch,
    uint16_t background,
    std::filesystem::path& outputPath) const {
  int next = 1;
  do {
    std::ostringstream name;
    name << "screenshot-" << std::setw(4) << std::setfill('0') << next++
         << ".png";
    outputPath = root_ / "exports" / name.str();
  } while (std::filesystem::exists(outputPath));

  SteamScreenshot::Image image;
  return SteamScreenshot::render(sketch, background, image) &&
      PngExport::writeRgb(
          outputPath, image.rgb, image.width, image.height);
}

bool Workspace::reloadUserPalettes() {
  userPalettes_.clear();
  std::vector<std::filesystem::path> candidates;
  std::error_code error;
  for (const auto& entry :
       std::filesystem::directory_iterator(root_ / "palettes", error)) {
    if (error) return false;
    if (entry.is_regular_file() && entry.path().extension() == ".hex") {
      candidates.push_back(entry.path());
    }
  }
  std::sort(candidates.begin(), candidates.end());
  for (const auto& path : candidates) {
    if (userPalettes_.size() >= kMaximumUserPalettes) break;
    std::ifstream input(path, std::ios::binary);
    const std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    Palette::Parsed parsed;
    if (!input.bad() &&
        Palette::parseLospecHex(contents.data(), contents.size(), parsed)) {
      UserPalette palette;
      palette.path = path;
      std::copy(
          std::begin(parsed.colors),
          std::end(parsed.colors),
          palette.colors.begin());
      palette.size = parsed.size;
      palette.name = path.stem().string();
      std::replace(palette.name.begin(), palette.name.end(), '-', ' ');
      std::replace(palette.name.begin(), palette.name.end(), '_', ' ');
      std::transform(
          palette.name.begin(),
          palette.name.end(),
          palette.name.begin(),
          [](unsigned char value) {
            return static_cast<char>(std::toupper(value));
          });
      if (contents.rfind("// Lospec palette\n", 0) == 0) {
        std::istringstream metadata(contents);
        std::string line;
        while (std::getline(metadata, line) && line.compare(0, 3, "// ") == 0) {
          if (line.compare(0, 9, "// Name: ") == 0 && line.size() > 9) {
            palette.name = line.substr(9, 80);
          } else if (line.compare(0, 11, "// Author: ") == 0) {
            palette.author = line.substr(11, 80);
          } else if (line.compare(0, 10, "// Likes: ") == 0) {
            std::istringstream count(line.substr(10));
            int likes = -1;
            if (count >> likes && likes >= 0) palette.likes = likes;
          }
        }
      }
      userPalettes_.push_back(std::move(palette));
    }
  }
  return true;
}

bool Workspace::deleteUserPalette(std::size_t index) {
  if (index >= userPalettes_.size()) return false;
  const auto source = userPalettes_[index].path;
  auto destination = root_ / "trash" / ("palette-" + source.filename().string());
  std::error_code error;
  int suffix = 1;
  while (std::filesystem::exists(destination, error)) {
    destination = root_ / "trash" /
        ("palette-" + source.stem().string() + "-" + std::to_string(suffix++) + ".hex");
  }
  if (error) return false;
  std::filesystem::rename(source, destination, error);
  if (error) return false;
  return reloadUserPalettes();
}

}  // namespace Desktop
}  // namespace bitmap16
