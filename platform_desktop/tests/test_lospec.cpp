#include "lospec.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "core/palette.h"
#include "core/palette_view.h"
#define PROGMEM
#include "cartridge_graphic.h"
#include "png_export.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

std::string palette(int index, int likes = 10, int count = 4) {
  std::string result = "{\"slug\":\"test-" + std::to_string(index) +
      "\",\"title\":\"Test palette " + std::to_string(index) +
      "\",\"user\":{\"name\":\"Pixel artist\"},\"likes\":" +
      std::to_string(likes) + ",\"colors\":[";
  constexpr const char* colors[] = {"574368", "8488d3", "cfd3c1", "f8c868"};
  for (int i = 0; i < count; ++i) {
    if (i) result += ',';
    result += "\"" + std::string(colors[i % 4]) + "\"";
  }
  return result + "]}";
}

std::string page(int start, int count, bool more, int likes = 10) {
  std::string result = "{\"data\":[";
  for (int i = 0; i < count; ++i) {
    if (i) result += ',';
    result += palette(start + i, likes);
  }
  return result + "],\"meta\":{\"hasMore\":" + (more ? "true" : "false") + "}}";
}

void renderPreview(const std::filesystem::path& path, bool dark) {
  bitmap16::Lospec::Page parsed;
  require(bitmap16::Lospec::parsePage(page(0, 3, false), parsed), "Preview fixture");
  std::vector<bitmap16::PaletteView::Entry> entries;
  for (const auto& item : parsed.palettes) {
    entries.push_back({item.colors.data(), item.title.c_str(), item.size,
                       false, item.author.c_str(), item.likes});
  }
  bitmap16::PaletteView::Catalog catalog{
      entries.data(), static_cast<int>(entries.size()), "LOSPEC / NEWEST"};
  bitmap16::PaletteView::State state;
  bitmap16::PaletteView::reset(state, catalog);
  bitmap16::PaletteView::selectCatalogIndex(state, 1);
  bitmap16::Canvas canvas;
  require(canvas.create(320, 170), "Preview canvas");
  const bitmap16::PaletteView::Theme theme = dark
      ? bitmap16::PaletteView::Theme{0x0000, 0xd69b, 0x7bef, true}
      : bitmap16::PaletteView::Theme{0xd69b, 0x0000, 0x7bef, false};
  bitmap16::PaletteView::render(canvas, state, catalog, -1, theme,
      CARTRIDGE_GRAPHIC, "R: refresh  +/-: 10 likes  L: local");
  std::vector<uint8_t> rgb;
  for (int i = 0; i < 320 * 170; ++i) {
    const uint16_t color = canvas.pixels()[i];
    rgb.push_back(static_cast<uint8_t>(((color >> 11) & 31) * 255 / 31));
    rgb.push_back(static_cast<uint8_t>(((color >> 5) & 63) * 255 / 63));
    rgb.push_back(static_cast<uint8_t>((color & 31) * 255 / 31));
  }
  require(bitmap16::Desktop::PngExport::writeRgb(path, rgb, 320, 170), "Preview PNG");
}

}  // namespace

int main(int argc, char** argv) {
  using namespace bitmap16::Lospec;
  if (argc == 2) {
    renderPreview(std::string(argv[1]) + "-light.png", false);
    renderPreview(std::string(argv[1]) + "-dark.png", true);
  }
  Page parsed;
  std::string slug;
  require(lookupSlug(" sweetie-16 ", slug) && slug == "sweetie-16", "Accept a short palette name");
  require(lookupSlug("https://lospec.com/palette-list/Sweetie-16?ref=test#colors", slug) &&
      slug == "sweetie-16", "Normalize a full Lospec URL");
  require(lookupSlug("https://www.lospec.com/palette-list/sweetie-16.json/", slug) &&
      slug == "sweetie-16", "Accept a direct download URL");
  for (const auto* invalid : {"", "../secret", "https://example.com/palette-list/test",
       "https://lospec.com.evil.test/palette-list/test", "https://lospec.com/palette-list/a/b",
       "hello world", "https://user@lospec.com/palette-list/test"}) {
    require(!lookupSlug(invalid, slug), "Reject invalid names and non-Lospec URLs");
  }
  Palette imported;
  std::string lookupError;
  const std::string lookupJson = "{\"name\":\"Test Four\",\"author\":\"Artist\","
      "\"colors\":[\"574368\",\"8488d3\",\"cfd3c1\",\"f8c868\"]}";
  require(parseLookup(lookupJson, "test-four", imported, lookupError) &&
      imported.size == 4 && imported.title == "Test Four" && imported.author == "Artist" &&
      imported.likes == -1, "Import direct-download format with attribution and no invented likes");
  require(!parseLookup("{\"error\":\"file not found\"}", "missing", imported, lookupError) &&
      lookupError == "Palette not found", "Report missing palettes");
  require(!parseLookup("{\"name\":\"Two\",\"colors\":[\"000000\",\"ffffff\"]}",
      "two", imported, lookupError) && lookupError == "Palettes need 4 to 16 colors",
      "Explain unsupported palette sizes");
  require(!parseLookup("<html>error</html>", "bad", imported, lookupError), "Reject non-JSON lookup responses");
  require(!parseLookup("{\"name\":\"Bad\",\"colors\":[\"zzzzzz\",\"ffffff\",\"ffffff\",\"ffffff\"]}",
      "bad", imported, lookupError), "Validate all imported colors");
  require(parsePage(page(0, 1, false), parsed), "Parse valid response");
  require(parsed.palettes.size() == 1 && parsed.palettes[0].size == 4 &&
      parsed.palettes[0].likes == 10 && parsed.palettes[0].author == "Pixel artist",
      "Parse colors and attribution");
  require(parsed.palettes[0].colors[0] ==
      bitmap16::Palette::rgb888ToRgb565(0x57, 0x43, 0x68), "RGB565 conversion");
  require(!parsePage("{bad", parsed), "Reject broken JSON");
  require(!parsePage("{\"data\":[]}", parsed), "Require pagination metadata");
  for (const int size : {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 32}) {
    require(parsePage("{\"data\":[" + palette(0, 10, size) +
        "],\"meta\":{\"hasMore\":false}}", parsed), "Parse palette page");
    require(parsed.palettes.size() == (size < 4 || size > 16 ? 0u : 1u),
        "Reject unsupported sizes rather than truncating");
  }
  std::string invalid = page(0, 1, false);
  invalid.replace(invalid.find("test-0"), 6, "../bad");
  require(parsePage(invalid, parsed) && parsed.palettes.empty(), "Reject unsafe filenames");
  invalid = page(0, 1, false);
  invalid.replace(invalid.find("574368"), 6, "nothex");
  require(parsePage(invalid, parsed) && parsed.palettes.empty(), "Validate every color");

  std::atomic_bool cancel{false};
  int calls = 0;
  auto result = refresh(10, {}, cancel, [&](int offset) {
    require(offset == 0, "One batch for twenty matches");
    ++calls;
    return Response{page(0, 100, true), ""};
  });
  require(result.error.empty() && result.calls == 1 && calls == 1 &&
      matching(result.palettes, 10, {}).size() == 20, "One request includes palette colors");
  require(matching(result.palettes, 11, {}).empty(), "Apply minimum likes locally");
  auto selected = availablePalettes(result.palettes, {});
  require(selected.size() == 100 && selected.back().slug == "test-99",
      "Expose every cached palette in one carousel");
  selected = availablePalettes(result.palettes, {"test-0"});
  require(selected.size() == 99 && selected.front().slug == "test-1",
      "Exclude saved palettes without pagination");
  require(availablePalettes(result.palettes, {}, 3).empty(),
      "Apply color count across the full carousel");
  auto unliked = result.palettes;
  unliked.front().likes = 0;
  require(availablePalettes(unliked, {}).size() == 100,
      "Do not filter palettes by likes");
  require(matching(result.palettes, 10, {"test-0"})[0].slug == "test-1", "Skip saved slugs");
  calls = 0;
  result = refresh(10, {}, cancel, [&](int offset) {
    require(offset == calls * 100, "Advance by full batch size");
    ++calls;
    return Response{page(offset, 100, true, 9), ""};
  });
  require(result.error.empty() && calls == 3 && result.calls == 3 &&
      matching(result.palettes, 10, {}).empty(), "Bound low-match refresh to three requests");
  result = refresh(10, {}, cancel, [](int) { return Response{page(0, 2, false), ""}; });
  require(result.calls == 1 && result.palettes.size() == 2, "Stop at catalog end");
  calls = 0;
  result = refresh(10, {}, cancel, [&](int) {
    ++calls;
    return Response{page(0, 1, true), ""};
  });
  require(calls == 3 && result.palettes.size() == 1, "Deduplicate across pages");
  result = refresh(10, {}, cancel, [](int) { return Response{"", "Rate limited"}; });
  require(result.error == "Rate limited" && result.calls == 1, "Do not retry failed requests");
  cancel.store(true);
  result = refresh(10, {}, cancel, [](int) { require(false, "Cancelled fetch ran"); return Response{}; });
  require(result.calls == 0 && !result.error.empty(), "Cancel before network access");
  cancel.store(false);
  require(!downloadPage(0, "bad\nkey", cancel).error.empty(), "Reject header injection");

  const auto root = std::filesystem::temp_directory_path() /
      ("bitmap16dx-lospec-test-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "palettes");
  require(!cacheFreshOn(root, "2026-11-19"), "Missing cache needs a daily refresh");
  std::ofstream(root / "lospec-cache.json") << page(0, 1, false);
  require(!cacheFreshOn(root, "2026-11-19"), "Legacy cache needs a dated refresh");
  const auto sizeRoot = root / "sizes";
  std::filesystem::create_directories(sizeRoot / "palettes");
  std::vector<Palette> sizedPalettes;
  for (int count = 4; count <= 16; ++count) {
    std::string json = "{\"name\":\"Variable\",\"colors\":[";
    for (int color = 0; color < count; ++color) {
      if (color) json += ',';
      json += "\"abcdef\"";
    }
    json += "]}";
    Palette sized;
    require(parseLookup(json, "size-" + std::to_string(count), sized, lookupError) &&
        sized.size == count, "Lookup preserves every supported palette size");
    require(savePalette(sizeRoot, sized), "Save arbitrary palette size as HEX");
    std::ifstream input(sizeRoot / "palettes" / ("lospec-" + sized.slug + ".hex"));
    const std::string contents((std::istreambuf_iterator<char>(input)), {});
    bitmap16::Palette::Parsed reopened;
    require(bitmap16::Palette::parseLospecHex(contents.data(), contents.size(), reopened) &&
        reopened.size == count, "Saved HEX retains its exact color count");
    // Feed entries carry a nonnegative likes count; direct lookups omit it.
    sized.likes = 0;
    sizedPalettes.push_back(sized);
  }
  require(availablePalettes(sizedPalettes, {}, 4).size() == 1 &&
      availablePalettes(sizedPalettes, {}, 8).size() == 4 &&
      availablePalettes(sizedPalettes, {}, 16).size() == 8,
      "Color filters partition palettes into 4, 5-8, and 9-16");
  require(saveCache(sizeRoot, sizedPalettes) && loadCache(sizeRoot).size() == 13,
      "Cache round-trips every supported palette size");
  require(writeApiKey(root, "test-key") && !writeApiKey(root, "bad\nkey"), "Validate stored key");
  const auto permissions = std::filesystem::status(root / "lospec-key.txt").permissions();
  require((permissions & (std::filesystem::perms::group_read |
      std::filesystem::perms::others_read)) == std::filesystem::perms::none,
      "Keep local key private");
  require(parsePage(page(0, 3, false), parsed) && saveCache(root, parsed.palettes), "Save cache");
  require(saveCache(root, parsed.palettes, "2026-11-19"), "Persist local refresh date");
  require(cacheFreshOn(root, "2026-11-19"), "Same-day cache stays fresh after reopening");
  require(!cacheFreshOn(root, "2026-11-20"), "Next local day needs a refresh");
  require(!cacheFreshOn(root, ""), "Unknown local date cannot mark cache fresh");
  const auto cached = loadCache(root);
  require(cached.size() == 3 && cached[0].hex == parsed.palettes[0].hex &&
      cached[0].author == "Pixel artist", "Round-trip exact colors and attribution offline");
  require(savePalette(root, cached[0]) && isSaved(root, cached[0]), "Save selected palette offline");
  require(savedSlugs(root) == std::vector<std::string>{"test-0"}, "Recognize saved palettes");
  require(matching(cached, 10, savedSlugs(root)).size() == 2, "Hide downloaded palettes");
  require(savePalette(root, cached[0]), "Repeated save is harmless");
  std::ifstream saved(root / "palettes" / "lospec-test-0.hex");
  std::string savedText((std::istreambuf_iterator<char>(saved)), {});
  bitmap16::Palette::Parsed colors;
  require(bitmap16::Palette::parseLospecHex(savedText.data(), savedText.size(), colors) &&
      colors.size == 4 && colors.colors[0] == cached[0].colors[0], "Reuse existing palette importer");
  std::ofstream(root / "lospec-cache.json") << "incomplete";
  require(loadCache(root).empty(), "Corrupt cache does not crash startup");
  require(!cacheFreshOn(root, "2026-11-19"), "Corrupt cache needs a refresh");
  require(currentLocalDay().size() == 10, "Local refresh date uses a calendar day");
  std::filesystem::remove_all(root);

  std::vector<bitmap16::PaletteView::Entry> entries(300);
  bitmap16::PaletteView::Catalog catalog{entries.data(), 300};
  {
    bitmap16::Lospec::Page parsed;
    require(parsePage(page(0, 3, false), parsed), "Landing fixture");
    std::vector<bitmap16::PaletteView::Entry> cartridges;
    for (const auto& palette : parsed.palettes) {
      cartridges.push_back({palette.colors.data(), palette.title.c_str(), palette.size});
    }
    cartridges[0].size = 5;
    cartridges[1].size = 6;
    cartridges[2].size = 7;
    const bitmap16::PaletteView::Catalog landingCatalog{cartridges.data(), 3};
    bitmap16::PaletteView::State landing;
    bitmap16::PaletteView::reset(landing, landingCatalog);
    landing.cursor = 1;
    bitmap16::Canvas canvas;
    require(canvas.create(320, 170), "Landing canvas");
    const bitmap16::PaletteView::Theme theme{0, 0xffff, 0x7777, true};
    landing.scrollPosition = 1.0f;
    bitmap16::PaletteView::render(canvas, landing, landingCatalog, -1, theme, CARTRIDGE_GRAPHIC);
    const std::vector<uint16_t> settled(canvas.pixels(), canvas.pixels() + 320 * 170);
    for (const float approach : {0.999f, 1.001f}) {
      landing.scrollPosition = approach;
      bitmap16::PaletteView::render(canvas, landing, landingCatalog, -1, theme, CARTRIDGE_GRAPHIC);
      require(std::equal(settled.begin(), settled.end(), canvas.pixels()),
          "Side cartridges must land with the center cartridge from either direction");
    }
    landing.scrollPosition = 1.0f;
    bitmap16::PaletteView::beginEntrance(landing, 320);
    const auto drawEntrance = [&]() {
      bitmap16::PaletteView::render(canvas, landing, landingCatalog, -1, theme, CARTRIDGE_GRAPHIC);
    };
    const auto visiblePixels = [&](int left, int right) {
      int count = 0;
      for (int y = 18; y < 132; ++y)
        for (int x = left; x < right; ++x)
          if (canvas.readPixel(x, y) != theme.background) ++count;
      return count;
    };
    drawEntrance();
    require(visiblePixels(20, 300) == 0, "Cartridges begin fully transparent");
    bitmap16::PaletteView::advanceTimed(landing, 60);
    drawEntrance();
    require(visiblePixels(120, 200) > 0 && visiblePixels(20, 100) == 0 &&
        visiblePixels(220, 300) == 0, "Center fades and rises before its neighbors");
    bitmap16::PaletteView::advanceTimed(landing, 120);
    drawEntrance();
    require(visiblePixels(20, 100) > 0 && visiblePixels(220, 300) > 0,
        "Entrance wave reaches both neighbors together");
    auto partitioned = landing;
    bitmap16::PaletteView::advanceTimed(landing, 32);
    bitmap16::PaletteView::advanceTimed(partitioned, 16);
    bitmap16::PaletteView::advanceTimed(partitioned, 16);
    require(std::fabs(landing.entranceElapsedMs - partitioned.entranceElapsedMs) < 0.001f,
        "Entrance timing is independent of frame rate");
    require(bitmap16::PaletteView::advanceTimed(landing, 1000) ==
        bitmap16::PaletteView::AnimationResult::Animating && !landing.entranceAnimating,
        "Present the final entrance frame");
    drawEntrance();
    require(std::equal(settled.begin(), settled.end(), canvas.pixels()),
        "Entrance settles exactly into the normal carousel");
    bitmap16::PaletteView::beginExit(landing, 320);
    drawEntrance();
    require(std::equal(settled.begin(), settled.end(), canvas.pixels()),
        "Exit begins exactly at the resting carousel");
    auto exitPartitioned = landing;
    bitmap16::PaletteView::advanceTimed(landing, 120);
    bitmap16::PaletteView::advanceTimed(exitPartitioned, 60);
    bitmap16::PaletteView::advanceTimed(exitPartitioned, 60);
    require(std::fabs(landing.exitElapsedMs - exitPartitioned.exitElapsedMs) < 0.001f,
        "Exit duration is independent of frame rate");
    require(!bitmap16::PaletteView::moveCursor(landing, 1) &&
        !bitmap16::PaletteView::beginSelection(landing), "Keep the departing list stable");
    require(bitmap16::PaletteView::advanceTimed(landing, 1000) ==
        bitmap16::PaletteView::AnimationResult::ExitComplete, "Exit reports completion");
    drawEntrance();
    require(visiblePixels(20, 300) == 0, "Every departing cartridge fades out completely");
    landing.exitAnimating = false;
    uint16_t replacementColors[16];
    std::fill(std::begin(replacementColors), std::end(replacementColors), 0xf800);
    auto replacementEntries = cartridges;
    replacementEntries[1].colors = replacementColors;
    replacementEntries[1].name = "RELOADED";
    const bitmap16::PaletteView::Catalog replacementCatalog{replacementEntries.data(), 3};
    bitmap16::PaletteView::render(canvas, landing, replacementCatalog, -1, theme, CARTRIDGE_GRAPHIC);
    const std::vector<uint16_t> replacement(canvas.pixels(), canvas.pixels() + 320 * 170);
    bitmap16::PaletteView::beginReload(landing, 320, settled.data());
    bitmap16::PaletteView::render(canvas, landing, replacementCatalog, -1, theme, CARTRIDGE_GRAPHIC);
    require(std::equal(settled.begin() + 18 * 320, settled.begin() + 157 * 320,
                       canvas.pixels() + 18 * 320), "Reload starts with the old cartridges intact");
    auto reloadPartitioned = landing;
    bitmap16::PaletteView::advanceTimed(landing, 100);
    bitmap16::PaletteView::advanceTimed(reloadPartitioned, 50);
    bitmap16::PaletteView::advanceTimed(reloadPartitioned, 50);
    require(std::fabs(landing.reloadElapsedMs - reloadPartitioned.reloadElapsedMs) < 0.001f,
        "Reload timing is independent of frame rate");
    bitmap16::PaletteView::advanceTimed(landing, 1000);
    bitmap16::PaletteView::render(canvas, landing, replacementCatalog, -1, theme, CARTRIDGE_GRAPHIC);
    require(!landing.reloadAnimating && landing.reloadFrame == nullptr &&
        std::equal(replacement.begin(), replacement.end(), canvas.pixels()),
        "Reload settles exactly into the new content and releases its snapshot");
    bitmap16::PaletteView::toggleSizeFilter(landing, landingCatalog, 4);
    require(landing.filteredCount == 0, "Filter can produce an empty list after exit");
    bitmap16::PaletteView::beginExit(landing, 320);
    require(bitmap16::PaletteView::advanceTimed(landing, 16) ==
        bitmap16::PaletteView::AnimationResult::ExitComplete, "Empty lists finish exit immediately");
    landing.exitAnimating = false;
    bitmap16::PaletteView::toggleSizeFilter(landing, landingCatalog, 4);
    bitmap16::PaletteView::selectCatalogIndex(landing, 1);
    bitmap16::PaletteView::beginEntrance(landing, 320);
    require(bitmap16::PaletteView::moveCursor(landing, 1) && !landing.entranceAnimating,
        "Navigation stays responsive during entrance");
  }
  bitmap16::PaletteView::State state;
  bitmap16::PaletteView::reset(state, catalog);
  bitmap16::PaletteView::selectCatalogIndex(state, 299);
  require(bitmap16::PaletteView::selectedCatalogIndex(state) == 299,
      "Reach every palette in a 300-entry carousel without truncating indices");
  bitmap16::PaletteView::State slow;
  slow.cursor = 1;
  auto fast = slow;
  bitmap16::PaletteView::advanceTimed(slow, 32.0f);
  bitmap16::PaletteView::advanceTimed(fast, 16.0f);
  bitmap16::PaletteView::advanceTimed(fast, 16.0f);
  require(std::fabs(slow.scrollPosition - fast.scrollPosition) < 0.00001f,
      "Scrolling covers the same distance at different frame rates");
  slow.scrollPosition = 0.995f;
  require(bitmap16::PaletteView::advanceTimed(slow, 16.0f) ==
      bitmap16::PaletteView::AnimationResult::Animating && slow.scrollPosition == 1.0f,
      "Present the final snapped frame before becoming idle");
  slow.insertionAnimating = true;
  slow.insertionProgress = 0.0f;
  fast = slow;
  bitmap16::PaletteView::advanceTimed(slow, 100.0f);
  for (int i = 0; i < 5; ++i) bitmap16::PaletteView::advanceTimed(fast, 20.0f);
  require(std::fabs(slow.insertionProgress - fast.insertionProgress) < 0.00001f,
      "Insertion duration is independent of frame rate");
  return 0;
}
