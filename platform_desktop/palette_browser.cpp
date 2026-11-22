#include "palette_browser.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "palettes.h"

namespace bitmap16 {
namespace Desktop {

#ifdef BITMAP16_LOSPEC
namespace {
PaletteBrowser::Downloads productionDownloads() {
  return {
      [](std::vector<std::string> saved, std::string key, const std::atomic_bool& cancel) {
        return std::async(std::launch::async,
            [saved = std::move(saved), key = std::move(key), &cancel]() {
              return Lospec::downloadLatest(0, saved, key, cancel);
            });
      },
      [](std::string input, const std::atomic_bool& cancel) {
        return std::async(std::launch::async, [input = std::move(input), &cancel]() {
          return Lospec::lookup(input, cancel);
        });
      },
      Lospec::usesSharedFeed(),
  };
}
}  // namespace

PaletteBrowser::PaletteBrowser(Workspace& workspace, Editor& editor)
    : PaletteBrowser(workspace, editor, productionDownloads()) {}

PaletteBrowser::PaletteBrowser(Workspace& workspace, Editor& editor, Downloads downloads)
    : workspace_(workspace), editor_(editor), downloads_(std::move(downloads)),
      cache_(Lospec::loadCache(workspace.root())) {
  reload();
}
#else
PaletteBrowser::PaletteBrowser(Workspace& workspace, Editor& editor)
    : workspace_(workspace), editor_(editor) {
  reload();
}
#endif

PaletteBrowser::~PaletteBrowser() { cancelAndWait(); }

const char* PaletteBrowser::status() const {
#ifdef BITMAP16_LOSPEC
  return status_.c_str();
#else
  return nullptr;
#endif
}

int PaletteBrowser::activeIndex() const {
  const auto& sketch = editor_.sketch();
  const auto matches = [&](const PaletteView::Entry& entry) {
    return entry.colors != nullptr && entry.size == sketch.paletteSize &&
        std::equal(entry.colors, entry.colors + kMaxPaletteColors, sketch.paletteColors);
  };
  // Identical palettes may have different names. Highlight the one being
  // inserted, rather than the first catalog entry with the same colors.
  const int selected = PaletteView::selectedCatalogIndex(state_);
  if (state_.insertionAnimating && selected >= 0 &&
      selected < static_cast<int>(entries_.size()) && matches(entries_[selected])) {
    return selected;
  }
  for (std::size_t i = 0; i < entries_.size(); ++i) {
    if (matches(entries_[i])) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void PaletteBrowser::open() {
  preview_ = false;
  online_ = false;
  closing_ = false;
  reload();
}

void PaletteBrowser::reload() {
  entries_.clear();
#ifdef BITMAP16_LOSPEC
  if (preview_) {
    for (const auto& palette : previewEntries_) {
      entries_.push_back({palette.colors.data(), palette.title.c_str(), palette.size,
                         false, palette.author.c_str(), palette.likes});
    }
    title_ = "IMPORT PREVIEW";
    catalog_.emptyMessage = "NO IMPORTED PALETTE";
#ifdef BITMAP16_STEAM_DECK
    status_ = "B:Cancel|A:Apply";
#else
    status_ = "Esc:Cancel|OK:Apply";
#endif
  } else if (online_) {
    onlineEntries_ = Lospec::availablePalettes(
        cache_, Lospec::savedSlugs(workspace_.root()), onlineFilter_);
    for (const auto& palette : onlineEntries_) {
      entries_.push_back({palette.colors.data(), palette.title.c_str(), palette.size,
                         false, palette.author.c_str(), palette.likes});
    }
    title_ = "PALETTES > LOSPEC (" + std::to_string(entries_.size()) + ')';
    catalog_.emptyMessage = cache_.empty() ? "NO CACHED PALETTES" : "NO NEW MATCHES";
#ifdef BITMAP16_STEAM_DECK
    status_ = "L4:Import";
#else
    status_ = "[I]mport";
#endif
  } else
#endif
  {
    workspace_.reloadUserPalettes();
    local_ = workspace_.userPalettes();
    for (int i = 0; i < NUM_PALETTES; ++i) {
      entries_.push_back({PALETTE_CATALOG[i], PALETTE_NAMES[i], PALETTE_SIZES[i], false});
    }
    for (const auto& palette : local_) {
      entries_.push_back({palette.colors.data(), palette.name.c_str(), palette.size,
                         true, palette.author.c_str(), palette.likes});
    }
    title_ = "PALETTES";
    catalog_.emptyMessage = "NO PALETTES";
#ifdef BITMAP16_STEAM_DECK
    status_ = "L4:Import|Browse Y:Lospec";
#else
    status_ = "[I]mport|Browse [L]ospec";
#endif
  }
  catalog_.entries = entries_.empty() ? nullptr : entries_.data();
  catalog_.count = static_cast<int>(entries_.size());
  catalog_.title = title_.c_str();
  PaletteView::reset(state_, catalog_);
#ifdef BITMAP16_LOSPEC
  if (online_ && !preview_ && onlineFilter_ != 0)
    PaletteView::toggleSizeFilter(state_, catalog_, onlineFilter_);
#endif
  const int active = activeIndex();
  if (active >= 0) PaletteView::selectCatalogIndex(state_, active);
}

void PaletteBrowser::request(Action action, const Canvas& frame, uint32_t now, uint8_t size) {
  if (closing_ || state_.insertionAnimating) return;
  if (preview_ && action != Action::Close && action != Action::SwitchList) return;
  if (action == Action::Close) {
    closing_ = true;
    if (!state_.entranceAnimating && !state_.reloadAnimating)
      PaletteView::beginExit(state_, frame.width());
    return;
  }
  reloadFrame_.assign(frame.pixels(), frame.pixels() + frame.width() * frame.height());
#ifdef BITMAP16_LOSPEC
  if (action == Action::SwitchList) {
    if (preview_) {
      preview_ = false;
      reload();
    } else {
      online_ = !online_;
      reload();
      if (online_ && !Lospec::cacheFreshOn(workspace_.root(), Lospec::currentLocalDay()))
        refresh(now);
    }
  } else if (online_ && action == Action::FilterSize) {
    onlineFilter_ = onlineFilter_ == size ? 0 : size;
    reload();
  } else
#else
  (void)now;
#endif
  if (action == Action::FilterUser) {
    PaletteView::toggleUserFilter(state_, catalog_);
  } else if (size == 0) {
    PaletteView::reset(state_, catalog_);
  } else {
    PaletteView::toggleSizeFilter(state_, catalog_, size);
  }
  PaletteView::beginReload(state_, frame.width(), reloadFrame_.data());
}

PaletteView::AnimationResult PaletteBrowser::advance(float elapsedMs, int viewportWidth) {
  if (closing_ && !state_.entranceAnimating && !state_.reloadAnimating && !state_.exitAnimating)
    PaletteView::beginExit(state_, viewportWidth);
  const auto result = PaletteView::advanceTimed(state_, elapsedMs);
  if (result == PaletteView::AnimationResult::ExitComplete) {
    closing_ = false;
    state_.exitAnimating = false;
  }
  return result;
}

bool PaletteBrowser::select() {
  if (closing_) return false;
  const int selected = PaletteView::selectedCatalogIndex(state_);
#ifdef BITMAP16_LOSPEC
  const auto& remoteEntries = preview_ ? previewEntries_ : onlineEntries_;
  if ((online_ || preview_) && !state_.insertionAnimating && selected >= 0 &&
      selected < static_cast<int>(remoteEntries.size()) && !save(remoteEntries[selected])) {
    return false;
  }
#endif
  if (!PaletteView::beginSelection(state_)) return false;
  if (selected >= 0 && selected < catalog_.count) {
    editor_.applyPalette(entries_[selected].colors, entries_[selected].size);
  }
  return true;
}

void PaletteBrowser::deleteSelection() {
  const int selected = PaletteView::selectedCatalogIndex(state_);
  if (online_ || preview_ || selected < NUM_PALETTES || selected >= catalog_.count || !entries_[selected].user) {
    status_ = "Built-in palettes cannot be deleted";
    return;
  }
  const auto previous = state_;
  const bool deleted = workspace_.deleteUserPalette(static_cast<std::size_t>(selected - NUM_PALETTES));
  reload();
  state_.filterSize = previous.filterSize;
  state_.userOnly = previous.userOnly;
  PaletteView::rebuildFilter(state_, catalog_);
  state_.cursor = std::max(0, std::min(previous.cursor, state_.filteredCount - 1));
  state_.scrollPosition = static_cast<float>(state_.cursor);
  status_ = deleted ? "Palette moved to trash" : "Could not delete palette";
}

bool PaletteBrowser::pending() const {
#ifdef BITMAP16_LOSPEC
  return request_.valid();
#else
  return false;
#endif
}

void PaletteBrowser::refresh(uint32_t now) {
#ifdef BITMAP16_LOSPEC
  if (preview_) return;
  const auto key = Lospec::readApiKey(workspace_.root());
  if (pending()) {
    status_ = "Getting newest palettes...";
  } else if (!downloads_.sharedFeed && key.empty()) {
    status_ = "Lospec key missing in build";
  } else if (refreshed_ && static_cast<uint32_t>(now - refreshedAt_) < 60000u) {
    status_ = "Please wait a minute to refresh";
  } else {
    refreshedAt_ = now;
    refreshed_ = true;
    importing_ = false;
    request_ = downloads_.latest(Lospec::savedSlugs(workspace_.root()), key, cancel_);
    status_ = "Getting newest palettes...";
  }
#else
  (void)now;
#endif
}

void PaletteBrowser::importPalette(const std::string& input) {
#ifdef BITMAP16_LOSPEC
  if (pending()) {
    status_ = "Wait for the current download";
    return;
  }
  importing_ = true;
  request_ = downloads_.lookup(input, cancel_);
  status_ = "Looking up palette...";
#else
  (void)input;
#endif
}

#ifdef BITMAP16_LOSPEC
bool PaletteBrowser::save(const Lospec::Palette& palette) {
  workspace_.reloadUserPalettes();
  if (workspace_.userPalettes().size() >= kMaximumUserPalettes &&
      !Lospec::isSaved(workspace_.root(), palette)) {
    status_ = "Local palette storage is full";
    return false;
  }
  if (!Lospec::savePalette(workspace_.root(), palette)) {
    status_ = "Could not save palette";
    return false;
  }
  return true;
}
#endif

PaletteBrowser::Update PaletteBrowser::poll(bool visible) {
#ifdef BITMAP16_LOSPEC
  if (!pending() || (visible && (state_.insertionAnimating || state_.reloadAnimating || closing_)) ||
      request_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
    return Update::None;
  }
  auto result = request_.get();
  const bool importing = importing_;
  importing_ = false;
  if (!result.error.empty()) {
    if (online_ || importing) status_ = result.error;
  } else if (importing) {
    if (result.palettes.empty()) {
      status_ = "Palette not found";
    } else {
      previewEntries_ = std::move(result.palettes);
      previewEntries_.resize(1);
      preview_ = true;
      closing_ = false;
      reload();
      return Update::PreviewReady;
    }
  } else {
    const bool cached = Lospec::saveCache(workspace_.root(), result.palettes);
    cache_ = std::move(result.palettes);
    if (online_) {
      reload();
      if (!cached) status_ = "Cache write failed";
    }
  }
  return Update::Changed;
#else
  (void)visible;
  return Update::None;
#endif
}

void PaletteBrowser::cancelAndWait() {
#ifdef BITMAP16_LOSPEC
  cancel_.store(true);
  if (request_.valid()) request_.wait();
#endif
}

}  // namespace Desktop
}  // namespace bitmap16
