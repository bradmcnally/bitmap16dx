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

std::array<uint8_t, 14> encodeSettings(const Settings& settings) {
  return {
      'B', '1', '6', 'S', 2,
      static_cast<uint8_t>(settings.theme),
      settings.defaultGridSize,
      settings.matrixUnits,
      settings.matrixRotation,
      static_cast<uint8_t>(settings.exportFormat),
      static_cast<uint8_t>(settings.shakeUndoEnabled ? 1 : 0),
      settings.displayBrightness,
      settings.matrixBrightness,
      static_cast<uint8_t>(settings.matrixEnabled ? 1 : 0),
  };
}

bool decodeSettings(
    const std::vector<uint8_t>& data,
    Settings& settings) {
  if (data.size() < 13 ||
      data[0] != 'B' || data[1] != '1' || data[2] != '6' ||
      data[3] != 'S' || (data[4] != 1 && data[4] != 2)) {
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
  std::error_code error;
  std::filesystem::create_directories(root_ / "sketches", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "exports", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "palettes", error);
  if (error) return false;
  std::filesystem::create_directories(root_ / "trash", error);
  if (error) return false;
  loadSettings();
  reloadUserPalettes();
  loadSketches();
  if (!sketches_.empty()) {
    activeIndex_ = 0;
    editor.reset(sketches_[0]);
  } else {
    newSketch(editor);
  }
  return true;
}

bool Workspace::loadSettings() {
  settings_ = normalizeSettings(Settings{});
  std::ifstream input(root_ / "settings.bin", std::ios::binary);
  if (!input) return false;
  const std::vector<uint8_t> data(
      (std::istreambuf_iterator<char>(input)),
      std::istreambuf_iterator<char>());
  return !input.bad() && decodeSettings(data, settings_);
}

bool Workspace::saveSettings() const {
  const auto data = encodeSettings(settings_);
  std::ofstream output(
      root_ / "settings.bin", std::ios::binary | std::ios::trunc);
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

bool Workspace::deleteSketch(std::size_t index, Editor& editor) {
  if (index >= sketchPaths_.size()) return false;
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
  loadSketches();
  if (sketches_.empty()) {
    newSketch(editor);
  } else {
    activeIndex_ = std::min<int>(
        static_cast<int>(index), static_cast<int>(sketches_.size()) - 1);
    editor.reset(sketches_[activeIndex_]);
  }
  return true;
}

bool Workspace::undoDelete(Editor& editor) {
  if (deletedOriginalPath_.empty() || deletedTrashPath_.empty() ||
      !std::filesystem::exists(deletedTrashPath_)) {
    return false;
  }
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
  deletedOriginalPath_.clear();
  deletedTrashPath_.clear();
  loadSketches();
  const auto found =
      std::find(sketchPaths_.begin(), sketchPaths_.end(), restored);
  if (found == sketchPaths_.end()) return false;
  activeIndex_ = static_cast<int>(found - sketchPaths_.begin());
  editor.reset(sketches_[activeIndex_]);
  return true;
}

bool Workspace::exportSketch(
    const Sketch& sketch,
    bool scaled,
    std::filesystem::path& outputPath) const {
  int next = 1;
  do {
    std::ostringstream name;
    name << "export-" << std::setw(4) << std::setfill('0') << next++
         << ".png";
    outputPath = root_ / "exports" / name.str();
  } while (std::filesystem::exists(outputPath));
  return PngExport::write(
      outputPath, sketch, scaled, settings_.exportFormat);
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
    if (userPalettes_.size() >= 20) break;
    std::ifstream input(path, std::ios::binary);
    const std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    Palette::Parsed parsed;
    if (!input.bad() &&
        Palette::parseLospecHex(contents.data(), contents.size(), parsed)) {
      UserPalette palette;
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
      userPalettes_.push_back(std::move(palette));
    }
  }
  return true;
}

}  // namespace Desktop
}  // namespace bitmap16
