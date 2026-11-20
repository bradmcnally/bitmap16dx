#include "palette_browser.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

using bitmap16::Desktop::PaletteBrowser;
using bitmap16::Lospec::Palette;
using bitmap16::Lospec::Result;

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void environment(const char* name, const std::string& value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

struct Fixture {
  std::filesystem::path root;
  bitmap16::Editor editor;
  bitmap16::Desktop::Workspace workspace;
  bitmap16::Canvas frame;
  Fixture() {
    static int sequence = 0;
    root = std::filesystem::temp_directory_path() /
        ("bitmap16dx-browser-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) +
         '-' + std::to_string(sequence++));
    environment("BITMAP16_DATA_DIR", root.string());
    environment("BITMAP16_CONFIG_DIR", "");
    environment("BITMAP16_LOSPEC_API_KEY", "");
    require(workspace.initialize(editor), "Initialize isolated workspace");
    require(frame.create(320, 170), "Create animation frame");
  }
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }
};

Palette palette(const std::string& slug, int size = 4) {
  std::string json = "{\"name\":\"Palette " + slug + "\",\"author\":\"Artist\",\"colors\":[";
  for (int i = 0; i < size; ++i) {
    if (i) json += ',';
    json += "\"123456\"";
  }
  json += "]}";
  Palette result;
  std::string error;
  require(bitmap16::Lospec::parseLookup(json, slug, result, error), "Create real palette fixture");
  result.likes = 10;
  return result;
}

struct Downloads {
  int latestCalls = 0;
  int lookupCalls = 0;
  std::string lastInput;
  std::vector<std::string> saved;
  std::promise<Result> completion;
  PaletteBrowser::Downloads adapter() {
    return {
        [this](std::vector<std::string> slugs, std::string, const std::atomic_bool&) {
          ++latestCalls;
          saved = std::move(slugs);
          completion = std::promise<Result>();
          return completion.get_future();
        },
        [this](std::string input, const std::atomic_bool&) {
          ++lookupCalls;
          lastInput = std::move(input);
          completion = std::promise<Result>();
          return completion.get_future();
        },
        true,
    };
  }
  void finish(std::vector<Palette> palettes = {}, const std::string& error = "") {
    completion.set_value({std::move(palettes), error, 1});
  }
};

void settle(PaletteBrowser& browser) { browser.advance(10000, 320); }
void browse(PaletteBrowser& browser, Fixture& fixture, uint32_t now = 100) {
  browser.request(PaletteBrowser::Action::SwitchList, fixture.frame, now);
  settle(browser);
}
void cache(Fixture& fixture, const std::vector<Palette>& palettes,
           const std::string& day = bitmap16::Lospec::currentLocalDay()) {
  require(bitmap16::Lospec::saveCache(fixture.root, palettes, day), "Seed cache");
}

void dailyRefreshAndFilters() {
  Fixture f;
  const auto small = palette("four");
  const auto medium = palette("six", 6);
  const auto large = palette("twelve", 12);
  cache(f, {small, medium, large});
  require(bitmap16::Lospec::savePalette(f.root, small), "Seed saved palette");
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  browse(b, f);
  require(b.online() && !b.pending() && d.latestCalls == 0,
          "Same-day cache opens without downloading");
  require(b.catalog().count == 2 && std::string(b.catalog().entries[0].name) == medium.title,
          "Browse all cached unsaved palettes in order");
  b.request(PaletteBrowser::Action::FilterSize, f.frame, 200, 8);
  settle(b);
  require(b.catalog().count == 1 && b.state().filterSize == 8,
          "Online size filter covers five through eight colors");
  browse(b, f);
  browse(b, f);
  require(b.catalog().count == 1 && b.state().filterSize == 8,
          "Online filter survives switching lists");
  cache(f, {small, medium, large}, "2000-01-01");
  browse(b, f);
  browse(b, f);
  require(b.pending() && d.latestCalls == 1 && d.saved == std::vector<std::string>{"four"},
          "A stale cache launches one refresh with saved slugs");
  b.importPalette("another");
  b.refresh(100);
  require(d.lookupCalls == 0 && d.latestCalls == 1, "One request excludes both request types");
  d.finish({small, medium, large});
  require(b.poll(true) == PaletteBrowser::Update::Changed && !b.pending(), "Finish daily refresh");
  require(bitmap16::Lospec::cacheFreshOn(f.root, bitmap16::Lospec::currentLocalDay()),
          "Completion records the current local calendar day");
}

void browsingAutomaticallyLoadsAndRetriesAnEmptyFeed() {
  Fixture f;
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  browse(b, f, 100);
  require(b.online() && b.pending() && d.latestCalls == 1 &&
          std::string(b.status()) == "Getting newest palettes...",
          "First Browse Lospec downloads automatically without a refresh action");
  d.finish({}, "Offline");
  b.poll(true);
  require(b.catalog().count == 0 && std::string(b.status()) == "Offline",
          "Failed first download reports its error");
  b.open();
  browse(b, f, 60100);
  require(b.pending() && d.latestCalls == 2,
          "Browsing again automatically retries a missing feed after cooldown");
  d.finish({palette("automatic")});
  require(b.poll(true) == PaletteBrowser::Update::Changed && b.catalog().count == 1,
          "Automatic download populates the carousel");
  b.open();
  browse(b, f, 120100);
  require(!b.pending() && d.latestCalls == 2 && b.catalog().count == 1,
          "A fresh feed opens immediately from cache without another request");
}

void cooldownWrapsAndIncludesFailures() {
  Fixture f;
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  const uint32_t start = std::numeric_limits<uint32_t>::max() - 1000u;
  b.refresh(start);
  d.finish({}, "Offline");
  b.poll(false);
  b.refresh(start + 59999u);
  require(d.latestCalls == 1 && !b.pending(), "Failed refresh still enforces cooldown across rollover");
  b.refresh(start + 60000u);
  require(d.latestCalls == 2 && b.pending(), "Refresh allowed after exactly one minute");
  d.finish();
  b.poll(false);
}

void completionRespectsViewAndAnimation() {
  for (int animation = 0; animation < 3; ++animation) {
    Fixture f;
    cache(f, {palette("old")});
    Downloads d;
    PaletteBrowser b(f.workspace, f.editor, d.adapter());
    browse(b, f);
    b.refresh(100);
    d.finish({palette("new")});
    if (animation == 0) b.state().insertionAnimating = true;
    if (animation == 1) b.state().reloadAnimating = true;
    if (animation == 2) b.request(PaletteBrowser::Action::Close, f.frame, 100);
    require(b.poll(true) == PaletteBrowser::Update::None && b.pending(),
            "Ready downloads wait while visible list is inserting, reloading or closing");
    require(b.poll(false) == PaletteBrowser::Update::Changed && !b.pending(),
            "Leaving palettes allows completion despite old animation state");
    require(std::string(b.catalog().entries[0].name) == "Palette new", "Completion rebuilds online catalog");
  }
  Fixture f;
  cache(f, {palette("old")});
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  browse(b, f);
  b.refresh(100);
  d.finish({palette("new")});
  bitmap16::PaletteView::beginEntrance(b.state(), 320);
  require(b.poll(true) == PaletteBrowser::Update::Changed, "Entrance does not block completion");
}

void importPreviewsBeforeSavingOrApplying() {
  Fixture f;
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  const auto item = palette("imported", 6);
  f.editor.setCursor(3, 2);
  require(f.editor.draw(), "Prepare artwork");
  b.importPalette("imported");
  require(d.lookupCalls == 1 && d.lastInput == "imported", "Lookup uses submitted name");
  d.finish({item});
  require(b.poll(false) == PaletteBrowser::Update::PreviewReady && b.preview(),
          "Lookup opens a preview even when completion occurs after leaving");
  require(b.catalog().count == 1 && std::string(b.catalog().title) == "IMPORT PREVIEW" &&
          b.catalog().entries[0].size == 6 && std::string(b.catalog().entries[0].author) == "Artist",
          "Preview shows the returned cartridge, color count, and author");
  require(!bitmap16::Lospec::isSaved(f.root, item) && f.editor.sketch().paletteSize == 16,
          "Lookup alone neither saves nor applies");
  b.request(PaletteBrowser::Action::FilterSize, f.frame, 100, 4);
  b.refresh(100);
  require(b.catalog().count == 1 && b.state().filteredCount == 1 && d.latestCalls == 0,
          "Filters and refresh cannot replace the preview");
  b.request(PaletteBrowser::Action::SwitchList, f.frame, 100);
  settle(b);
  require(!b.preview() && !b.online() && !bitmap16::Lospec::isSaved(f.root, item) &&
          f.editor.sketch().paletteSize == 16, "Cancel returns to local palettes without changing artwork");
  b.importPalette("imported");
  d.finish({item});
  require(b.poll(true) == PaletteBrowser::Update::PreviewReady && b.select(),
          "Confirming the preview applies its cartridge");
  require(bitmap16::Lospec::isSaved(f.root, item) && f.editor.sketch().paletteSize == 6 &&
          f.editor.sketch().pixels[2][3] == 1 && f.editor.undo() &&
          f.editor.sketch().paletteSize == 16, "Confirmed import saves and supports undo");
  b.open();
  b.importPalette("missing");
  d.finish();
  require(b.poll(true) == PaletteBrowser::Update::Changed && std::string(b.status()) == "Palette not found",
          "Empty lookup reports not found");
  b.importPalette("offline");
  d.finish({}, "Offline");
  b.poll(true);
  require(std::string(b.status()) == "Offline" && f.editor.sketch().paletteSize == 16,
          "Lookup errors leave the sketch unchanged");
}

void previewCancellationRestoresTheFeed() {
  Fixture f;
  const auto feed = palette("feed", 6);
  const auto imported = palette("lookup", 12);
  cache(f, {feed});
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  browse(b, f);
  b.request(PaletteBrowser::Action::FilterSize, f.frame, 100, 8);
  settle(b);
  b.importPalette("lookup");
  d.finish({imported});
  require(b.poll(true) == PaletteBrowser::Update::PreviewReady && b.preview(), "Preview from the feed");
  b.request(PaletteBrowser::Action::SwitchList, f.frame, 100);
  settle(b);
  require(!b.preview() && b.online() && b.catalog().count == 1 &&
          b.state().filterSize == 8 && !bitmap16::Lospec::isSaved(f.root, imported),
          "Cancel restores the source feed and its filter without saving the lookup");
  require(b.select() && f.editor.sketch().paletteSize == 6 &&
          bitmap16::Lospec::isSaved(f.root, feed), "Feed cartridges still apply directly");
}

void saveFailureBlocksImportAndFeedSelection() {
  Fixture f;
  const auto item = palette("blocked");
  cache(f, {item});
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  // A directory at the target filename fails on every OS, even as root.
  std::filesystem::create_directory(f.root / "palettes" / "lospec-blocked.hex");
  b.importPalette("blocked");
  d.finish({item});
  require(b.poll(true) == PaletteBrowser::Update::PreviewReady && !b.select() &&
          std::string(b.status()) == "Could not save palette" && f.editor.sketch().paletteSize == 16,
          "Failed import save cannot apply a palette");
  b.open();
  browse(b, f);
  require(!b.select() && !b.state().insertionAnimating && f.editor.sketch().paletteSize == 16,
          "Failed feed save cannot begin selection or apply a palette");
  std::filesystem::remove(f.root / "palettes" / "lospec-blocked.hex");
  require(b.select() && bitmap16::Lospec::isSaved(f.root, item) && f.editor.sketch().paletteSize == 4,
          "Feed selection saves before applying");
}

void capacityAndCatalogOwnership() {
  Fixture f;
  const auto saved = palette("already-saved");
  const auto fresh = palette("new");
  for (std::size_t i = 0; i < bitmap16::Desktop::kMaximumUserPalettes; ++i) {
    require(bitmap16::Lospec::savePalette(f.root, i == 0 ? saved : palette("saved-" + std::to_string(i))),
            "Fill valid palette storage");
  }
  cache(f, {saved, fresh});
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  const auto* entries = b.catalog().entries;
  const int count = b.catalog().count;
  const std::string name = entries[count - 1].name;
  const uint16_t color = entries[count - 1].colors[0];
  f.workspace.reloadUserPalettes();
  require(std::string(entries[count - 1].name) == name && entries[count - 1].colors[0] == color,
          "Catalog owns local strings and colors across Workspace reloads");
  b.importPalette("new");
  d.finish({fresh});
  require(b.poll(true) == PaletteBrowser::Update::PreviewReady && !b.select(),
          "Full storage still permits preview, but confirmation fails");
  require(std::string(b.status()) == "Local palette storage is full" &&
          !bitmap16::Lospec::isSaved(f.root, fresh) && f.editor.sketch().paletteSize == 16,
          "Full storage blocks new imports without changing artwork");
  b.open();
  browse(b, f);
  require(!b.select() && f.editor.sketch().paletteSize == 16,
          "Full storage also blocks applying a new feed entry");
  b.importPalette("already-saved");
  d.finish({saved});
  require(b.poll(true) == PaletteBrowser::Update::PreviewReady && b.select() && f.editor.sketch().paletteSize == 4,
          "An already saved slug can be imported at capacity");
}

void localDeletionPreservesFiltersAndSketch() {
  Fixture f;
  require(bitmap16::Lospec::savePalette(f.root, palette("a")) &&
          bitmap16::Lospec::savePalette(f.root, palette("b")), "Seed two identical local palettes");
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  b.request(PaletteBrowser::Action::FilterUser, f.frame, 100);
  settle(b);
  b.request(PaletteBrowser::Action::FilterSize, f.frame, 100, 4);
  settle(b);
  require(bitmap16::PaletteView::moveCursor(b.state(), 1), "Select second saved palette");
  const int selected = bitmap16::PaletteView::selectedCatalogIndex(b.state());
  require(b.select() && b.activeIndex() == selected,
          "Selection highlights the chosen name when palettes have identical colors");
  b.state().insertionAnimating = false;
  b.deleteSelection();
  require(b.state().userOnly && b.state().filterSize == 4 && b.state().filteredCount == 1 &&
          b.state().cursor == 0 && f.editor.sketch().paletteSize == 4,
          "Deleting saved palettes retains filters, clamps selection and preserves the sketch");
  b.request(PaletteBrowser::Action::FilterSize, f.frame, 100, 0);
  settle(b);
  bitmap16::PaletteView::selectCatalogIndex(b.state(), 0);
  const int count = b.catalog().count;
  b.deleteSelection();
  require(b.catalog().count == count && std::string(b.status()) == "Built-in palettes cannot be deleted",
          "Built-in palettes remain protected");
}

void cacheFailuresPreserveOfflineBrowsing() {
  Fixture f;
  cache(f, {palette("old")});
  Downloads d;
  PaletteBrowser b(f.workspace, f.editor, d.adapter());
  browse(b, f);
  b.refresh(100);
  d.finish({}, "Offline");
  b.poll(true);
  require(b.catalog().count == 1 && std::string(b.catalog().entries[0].name) == "Palette old",
          "Failed download preserves cached browsing");
  std::filesystem::remove(f.root / "lospec-cache.json");
  std::filesystem::create_directory(f.root / "lospec-cache.json");
  b.refresh(60100);
  d.finish({palette("new")});
  b.poll(true);
  require(std::string(b.status()) == "Cache write failed" &&
          std::string(b.catalog().entries[0].name) == "Palette new",
          "Disk cache failure still replaces the in-memory feed");
}

void cancellationWaitsForWorkerLifetime() {
  Fixture f;
  std::atomic_bool observed{false};
  Downloads d;
  auto downloads = d.adapter();
  downloads.latest = [&observed](std::vector<std::string>, std::string, const std::atomic_bool& cancel) {
    return std::async(std::launch::async, [&observed, &cancel]() {
      while (!cancel.load()) std::this_thread::yield();
      observed.store(true);
      return Result{};
    });
  };
  {
    PaletteBrowser b(f.workspace, f.editor, std::move(downloads));
    b.refresh(100);
  }
  require(observed.load(), "Destruction cancels and joins before its flag is destroyed");
}
}  // namespace

int main() {
#ifdef BITMAP16_STEAM_DECK
  {
    Fixture f;
    Downloads d;
    PaletteBrowser browser(f.workspace, f.editor, d.adapter());
    require(std::string(browser.status()) == "L3:Import|Browse Y:Lospec",
            "Deck keeps Import left and Browse Lospec right like Zero");
  }
#endif
  dailyRefreshAndFilters();
  browsingAutomaticallyLoadsAndRetriesAnEmptyFeed();
  cooldownWrapsAndIncludesFailures();
  completionRespectsViewAndAnimation();
  importPreviewsBeforeSavingOrApplying();
  previewCancellationRestoresTheFeed();
  saveFailureBlocksImportAndFeedSelection();
  capacityAndCatalogOwnership();
  localDeletionPreservesFiltersAndSketch();
  cacheFailuresPreserveOfflineBrowsing();
  cancellationWaitsForWorkerLifetime();
  std::cout << "11 palette browser scenarios passed\n";
}
