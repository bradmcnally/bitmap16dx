#include <cstdlib>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "core/editor.h"
#include "core/settings.h"
#include "workspace.h"

int main() {
#ifdef _WIN32
  const int processId = _getpid();
#else
  const int processId = getpid();
#endif
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("bitmap16dx-workspace-regression-" +
       std::to_string(processId));
  std::error_code error;
  std::filesystem::remove_all(root, error);
#ifdef _WIN32
  _putenv_s("BITMAP16_DATA_DIR", root.string().c_str());
  _putenv_s("BITMAP16_CONFIG_DIR", "");
#else
  setenv("BITMAP16_DATA_DIR", root.string().c_str(), 1);
  unsetenv("BITMAP16_CONFIG_DIR");
#endif

  bitmap16::Editor firstEditor;
  bitmap16::Desktop::Workspace first;
  if (!first.initialize(firstEditor) || !first.sketches().empty()) return 1;
  firstEditor.setCursor(3, 2);
  firstEditor.setSelectedColor(4);
  if (!firstEditor.draw() || !first.saveSketch(firstEditor, true)) return 2;
  std::filesystem::rename(
      root / "sketches" / "sketch_1.dat",
      root / "sketches" / "sketch_100.dat",
      error);
  if (error) return 2;
  first.settings().theme = bitmap16::ThemeId::Dark;
  first.settings().cursorStyle = bitmap16::CursorStyle::Hand;
  first.settings().defaultGridSize = 16;
  first.settings().matrixEnabled = true;
  first.settings().batteryDisplay = false;
  first.settings().displayWarmth = 2;
  if (!first.saveSettings()) return 3;

  bitmap16::Editor restoredEditor;
  bitmap16::Desktop::Workspace restored;
  if (!restored.initialize(restoredEditor)) return 4;
  if (restored.sketches().size() != 1 ||
      restored.activeIndex() != -1 ||
      restoredEditor.sketch().pixels[2][3] != 0 ||
      restored.settings().theme != bitmap16::ThemeId::Dark ||
      restored.settings().cursorStyle != bitmap16::CursorStyle::Hand ||
      restored.settings().defaultGridSize != 16 ||
      !restored.settings().matrixEnabled ||
      restored.settings().batteryDisplay ||
      restored.settings().displayWarmth != 2) {
    return 5;
  }
  if (!restored.openSketch(0, restoredEditor) ||
      restoredEditor.sketch().pixels[2][3] != 4) {
    return 5;
  }
  if (!restored.saveSketch(restoredEditor, false) ||
      !std::filesystem::exists(
          root / "sketches" / "sketch_101.dat")) {
    return 5;
  }

  restored.newSketch(restoredEditor);
  restoredEditor.toggleGridSize();
  restoredEditor.setCursor(31, 31);
  restoredEditor.setSelectedColor(3);
  if (!restoredEditor.draw() ||
      restoredEditor.sketch().gridSize != 32 ||
      !restored.saveSketch(restoredEditor, true) ||
      restored.sketches().size() != 2 ||
      restored.sketches()[0].gridSize != 32 ||
      !std::filesystem::exists(
          root / "sketches" / "sketch_102.dat")) {
    return 6;
  }

  std::filesystem::path exported;
  if (!restored.exportSketch(restoredEditor.sketch(), true, exported) ||
      !std::filesystem::exists(exported)) {
    return 7;
  }
  std::ifstream png(exported, std::ios::binary);
  unsigned char signature[8] = {};
  png.read(reinterpret_cast<char*>(signature), 8);
  if (png.gcount() != 8 || signature[0] != 137 ||
      signature[1] != 'P' || signature[2] != 'N' ||
      signature[3] != 'G') {
    return 8;
  }
  unsigned char ihdr[16] = {};
  png.read(reinterpret_cast<char*>(ihdr), 16);
  if (png.gcount() != 16 || ihdr[4] != 'I' || ihdr[5] != 'H' ||
      ihdr[6] != 'D' || ihdr[7] != 'R' ||
      ihdr[11] != 128 || ihdr[15] != 128) {
    return 8;
  }

  std::filesystem::path screenshot;
  if (!restored.exportSteamScreenshot(
          restoredEditor.sketch(), 0x1111, screenshot) ||
      !std::filesystem::exists(screenshot)) {
    return 9;
  }
  std::ifstream screenshotPng(screenshot, std::ios::binary);
  screenshotPng.read(reinterpret_cast<char*>(signature), 8);
  screenshotPng.read(reinterpret_cast<char*>(ihdr), 16);
  if (screenshotPng.gcount() != 16 ||
      ihdr[8] != 0 || ihdr[9] != 0 ||
      ihdr[10] != 5 || ihdr[11] != 0 ||
      ihdr[12] != 0 || ihdr[13] != 0 ||
      ihdr[14] != 3 || ihdr[15] != 32) {
    return 9;
  }

  // Catalog operations must never replace or reset the open document.
  restoredEditor.setCursor(0, 0);
  restoredEditor.setSelectedColor(2);
  if (!restoredEditor.draw()) return 10;
  const uint8_t openPixel = restoredEditor.sketch().pixels[0][0];
  if (!restored.duplicateSketch(1) ||
      restored.sketches().size() != 3 ||
      restoredEditor.sketch().pixels[0][0] != openPixel) {
    return 10;
  }
  if (!restored.deleteSketch(0) ||
      restored.sketches().size() != 2 ||
      restoredEditor.sketch().pixels[0][0] != openPixel ||
      !restored.undoDelete() ||
      restored.sketches().size() != 3 ||
      restoredEditor.sketch().pixels[0][0] != openPixel) {
    return 10;
  }
  const int activeBeforeDelete = restored.activeIndex();
  if (activeBeforeDelete < 0 ||
      !restored.deleteSketch(static_cast<std::size_t>(activeBeforeDelete)) ||
      restored.activeIndex() != -1 ||
      restoredEditor.sketch().pixels[0][0] != openPixel ||
      !restored.undoDelete() ||
      restored.activeIndex() < 0 ||
      restoredEditor.sketch().pixels[0][0] != openPixel) {
    return 10;
  }

  std::filesystem::path palettePath = root / "palettes" / "test-palette.hex";
  {
    std::ofstream palette(palettePath);
    palette << "ff0000\n00ff00\n0000ff\nffffff\n";
  }
  if (!restored.reloadUserPalettes() ||
      restored.userPalettes().size() != 1 ||
      restored.userPalettes()[0].size != 4 ||
      restored.userPalettes()[0].name != "TEST PALETTE") {
    return 11;
  }
  {
    std::ofstream palette(root / "palettes" / "lospec-sample.hex");
    palette << "// Lospec palette\n// Name: A real palette title\n"
            << "// Author: Palette creator\n// Likes: 42\n"
            << "ff0000\n00ff00\n0000ff\nffffff\n";
  }
  if (!restored.reloadUserPalettes() || restored.userPalettes().size() != 2 ||
      restored.userPalettes()[0].name != "A real palette title" ||
      restored.userPalettes()[0].author != "Palette creator" ||
      restored.userPalettes()[0].likes != 42) {
    return 13;
  }

  restoredEditor.applyPalette(restored.userPalettes()[0].colors.data(), 4);
  if (!restored.saveSketch(restoredEditor, true)) return 14;
  const auto beforePaletteDelete = restoredEditor.sketch();
  if (!restored.deleteUserPalette(0) || restored.userPalettes().size() != 1 ||
      std::filesystem::exists(root / "palettes" / "lospec-sample.hex") ||
      !std::filesystem::exists(root / "trash" / "palette-lospec-sample.hex") ||
      restored.deleteUserPalette(99)) {
    return 14;
  }
  for (int color = 0; color < 16; ++color) {
    if (restoredEditor.sketch().paletteColors[color] != beforePaletteDelete.paletteColors[color]) return 15;
  }
  bitmap16::Editor reopenedEditor;
  bitmap16::Desktop::Workspace reopened;
  if (!reopened.initialize(reopenedEditor) || !reopened.openSketch(0, reopenedEditor)) return 16;
  for (int color = 0; color < 16; ++color) {
    if (reopenedEditor.sketch().paletteColors[color] != beforePaletteDelete.paletteColors[color]) return 16;
  }

  // Older version 4 files with the battery option default to neutral warmth.
  const auto settingsPath = root / "settings.bin";
  std::filesystem::resize_file(settingsPath, 17, error);
  bitmap16::Editor previousEditor;
  bitmap16::Desktop::Workspace previous;
  if (error || !previous.initialize(previousEditor) ||
      previous.settings().batteryDisplay ||
      previous.settings().displayWarmth != 0) {
    return 17;
  }

  // Existing version 4 files can also end after the cursor style.
  std::filesystem::resize_file(settingsPath, 16, error);
  bitmap16::Editor legacyEditor;
  bitmap16::Desktop::Workspace legacy;
  if (error || !legacy.initialize(legacyEditor) ||
      !legacy.settings().batteryDisplay ||
      legacy.settings().theme != bitmap16::ThemeId::Dark ||
      legacy.settings().displayWarmth != 0) {
    return 12;
  }
#ifdef BITMAP16_CARDPUTER_ZERO
  // Configuration is separate from artwork and does not import old data-folder settings.
  const auto config = root / "config";
#ifdef _WIN32
  _putenv_s("BITMAP16_CONFIG_DIR", config.string().c_str());
#else
  setenv("BITMAP16_CONFIG_DIR", config.string().c_str(), 1);
#endif
  bitmap16::Editor configuredEditor;
  bitmap16::Desktop::Workspace configured;
  if (!configured.initialize(configuredEditor) || configured.configRoot() != config ||
      configured.settings().theme != bitmap16::Settings{}.theme ||
      std::filesystem::exists(config / "settings.bin") ||
      !std::filesystem::is_regular_file(root / "settings.bin")) return 18;
  configured.settings().theme = bitmap16::ThemeId::Dark;
  if (!configured.saveSettings()) return 19;
  bitmap16::Desktop::Workspace preferred;
  if (!preferred.initialize(configuredEditor) ||
      preferred.settings().theme != bitmap16::ThemeId::Dark) return 20;
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  // Device exports belong in the user's visible Pictures folder.
  const auto deviceHome = root / "device-home";
  std::filesystem::create_directories(deviceHome, error);
  if (error) return 21;
  unsetenv("BITMAP16_DATA_DIR");
  unsetenv("XDG_DATA_HOME");
  setenv("HOME", deviceHome.string().c_str(), 1);
  bitmap16::Editor deviceEditor;
  bitmap16::Desktop::Workspace deviceWorkspace;
  std::filesystem::path deviceExport;
  if (!deviceWorkspace.initialize(deviceEditor) ||
      !deviceWorkspace.exportSketch(deviceEditor.sketch(), true, deviceExport) ||
      deviceExport != deviceHome / "Pictures" / "BitMap16DX" / "export-0001.png" ||
      !std::filesystem::is_regular_file(deviceExport)) return 21;
#endif
  if (std::getenv("BITMAP16_KEEP_TEST_DATA") == nullptr) {
    std::filesystem::remove_all(root, error);
  }
  return 0;
}
#include <fstream>
