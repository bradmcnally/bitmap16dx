#include "lospec.h"

#include <chrono>
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
  require(parsePage(page(0, 1, false), parsed), "Parse valid response");
  require(parsed.palettes.size() == 1 && parsed.palettes[0].size == 4 &&
      parsed.palettes[0].likes == 10 && parsed.palettes[0].author == "Pixel artist",
      "Parse colors and attribution");
  require(parsed.palettes[0].colors[0] ==
      bitmap16::Palette::rgb888ToRgb565(0x57, 0x43, 0x68), "RGB565 conversion");
  require(!parsePage("{bad", parsed), "Reject broken JSON");
  require(!parsePage("{\"data\":[]}", parsed), "Require pagination metadata");
  for (const int size : {4, 8, 16, 5, 32}) {
    require(parsePage("{\"data\":[" + palette(0, 10, size) +
        "],\"meta\":{\"hasMore\":false}}", parsed), "Parse palette page");
    require(parsed.palettes.size() == (size == 5 || size == 32 ? 0u : 1u),
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
  require(readMinimumLikes(root) == 10 && writeMinimumLikes(root, 25) &&
      readMinimumLikes(root) == 25, "Persist threshold");
  require(!writeMinimumLikes(root, -1), "Reject invalid threshold");
  require(writeApiKey(root, "test-key") && !writeApiKey(root, "bad\nkey"), "Validate stored key");
  const auto permissions = std::filesystem::status(root / "lospec-key.txt").permissions();
  require((permissions & (std::filesystem::perms::group_read |
      std::filesystem::perms::others_read)) == std::filesystem::perms::none,
      "Keep local key private");
  require(parsePage(page(0, 3, false), parsed) && saveCache(root, parsed.palettes), "Save cache");
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
  std::filesystem::remove_all(root);

  std::vector<bitmap16::PaletteView::Entry> entries(128);
  bitmap16::PaletteView::Catalog catalog{entries.data(), 128};
  bitmap16::PaletteView::State state;
  bitmap16::PaletteView::reset(state, catalog);
  bitmap16::PaletteView::selectCatalogIndex(state, 127);
  require(bitmap16::PaletteView::selectedCatalogIndex(state) == 127,
      "Zero can reach downloaded palettes beyond the original twenty");
  return 0;
}
