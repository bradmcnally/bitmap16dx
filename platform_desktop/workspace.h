#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include "core/editor.h"
#include "core/settings.h"
#include "core/sketch_workflow.h"

namespace bitmap16 {
namespace Desktop {

#if defined(BITMAP16_CARDPUTER_ZERO) || defined(BITMAP16_STEAM_DECK)
constexpr std::size_t kMaximumUserPalettes = 116;
#else
constexpr std::size_t kMaximumUserPalettes = 20;
#endif

class Workspace {
 public:
  struct UserPalette {
    std::array<uint16_t, 16> colors = {};
    uint8_t size = 0;
    std::string name;
    std::string author;
    int likes = -1;
    std::filesystem::path path;
  };

  bool initialize(Editor& editor);

  const std::filesystem::path& root() const { return root_; }
  const std::filesystem::path& configRoot() const { return configRoot_; }
  const Settings& settings() const { return settings_; }
  Settings& settings() { return settings_; }
  const std::vector<Sketch>& sketches() const { return sketches_; }
  const std::vector<UserPalette>& userPalettes() const {
    return userPalettes_;
  }
  int activeIndex() const { return activeIndex_; }

  bool newSketch(Editor& editor);
  bool openSketch(std::size_t index, Editor& editor);
  bool openSketch(const std::string& key, Editor& editor);
  std::string sketchKey(std::size_t index) const;
  bool hasDetachedSketch() const { return !detachedActivePath_.empty(); }
  bool saveSketch(const Editor& editor, bool saveAsNew);
  bool duplicateSketch(std::size_t index);
  bool deleteSketch(std::size_t index);
  bool undoDelete();
  bool exportSketch(
      const Sketch& sketch,
      bool scaled,
      std::filesystem::path& outputPath) const;
  bool exportSteamScreenshot(
      const Sketch& sketch,
      uint16_t background,
      std::filesystem::path& outputPath) const;
  bool reloadUserPalettes();
  bool deleteUserPalette(std::size_t index);
  bool saveSettings() const;

 private:
  bool loadSettings();
  bool loadSketches();
  bool writeSketch(const std::filesystem::path& path, const Sketch& sketch);

  std::filesystem::path root_;
  std::filesystem::path configRoot_;
  std::vector<std::filesystem::path> sketchPaths_;
  std::vector<Sketch> sketches_;
  std::vector<UserPalette> userPalettes_;
  std::filesystem::path deletedOriginalPath_;
  std::filesystem::path deletedTrashPath_;
  bool deletedWasActive_ = false;
  std::filesystem::path detachedActivePath_;
  Settings settings_;
  int activeIndex_ = -1;
};

class SketchStorage final : public SketchWorkflow::Storage {
 public:
  SketchStorage(Workspace& workspace, Editor& editor) : workspace_(workspace), editor_(editor) {}
  bool save(bool asNew) override;
  bool create() override;
  bool open(const std::string& key) override;
  std::string currentKey() const override;

 private:
  Workspace& workspace_;
  Editor& editor_;
};

}  // namespace Desktop
}  // namespace bitmap16
