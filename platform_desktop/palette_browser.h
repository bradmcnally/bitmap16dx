#pragma once

#include <string>
#include <vector>

#include "core/canvas.h"
#include "core/palette_view.h"
#include "workspace.h"
#ifdef BITMAP16_LOSPEC
#include <atomic>
#include <functional>
#include <future>
#include "lospec.h"
#endif

namespace bitmap16 {
namespace Desktop {

// Owns catalog backing data and the decisions shared by every palette input path.
// SDL supplies input, frame timing and presentation; downloads never touch UI state.
class PaletteBrowser {
 public:
  enum class Action { SwitchList, FilterSize, FilterUser, Close };
  enum class Update { None, Changed, PreviewReady };
#ifdef BITMAP16_LOSPEC
  struct Downloads {
    std::function<std::future<Lospec::Result>(
        std::vector<std::string>, std::string, const std::atomic_bool&)> latest;
    std::function<std::future<Lospec::Result>(
        std::string, const std::atomic_bool&)> lookup;
    bool sharedFeed = true;
  };
  PaletteBrowser(Workspace& workspace, Editor& editor, Downloads downloads);
#endif
  PaletteBrowser(Workspace& workspace, Editor& editor);
  ~PaletteBrowser();

  PaletteView::State& state() { return state_; }
  const PaletteView::Catalog& catalog() const { return catalog_; }
  int activeIndex() const;
  const char* status() const;
  bool online() const { return online_ || preview_; }
  bool preview() const { return preview_; }
  bool closing() const { return closing_; }
  bool pending() const;

  void open();
  void request(Action action, const Canvas& frame, uint32_t now, uint8_t size = 0);
  PaletteView::AnimationResult advance(float elapsedMs, int viewportWidth);
  bool select();
  void deleteSelection();
  void refresh(uint32_t now);
  void importPalette(const std::string& input);
  Update poll(bool visible);
  void cancelAndWait();

 private:
  void reload();
  Workspace& workspace_;
  Editor& editor_;
  PaletteView::State state_;
  PaletteView::Catalog catalog_;
  // Snapshot local data: Workspace reloads must not invalidate rendered entries.
  std::vector<Workspace::UserPalette> local_;
  std::vector<PaletteView::Entry> entries_;
  std::vector<uint16_t> reloadFrame_;
  std::string title_;
  std::string status_;
  bool online_ = false;
  bool preview_ = false;
  bool closing_ = false;
#ifdef BITMAP16_LOSPEC
  Downloads downloads_;
  std::vector<Lospec::Palette> cache_;
  std::vector<Lospec::Palette> onlineEntries_;
  std::vector<Lospec::Palette> previewEntries_;
  std::atomic_bool cancel_{false};
  std::future<Lospec::Result> request_;
  bool importing_ = false;
  bool refreshed_ = false;
  uint32_t refreshedAt_ = 0;
  uint8_t onlineFilter_ = 0;
  bool save(const Lospec::Palette& palette);
#endif
};

}  // namespace Desktop
}  // namespace bitmap16
