#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "core/editor.h"
#include "core/settings.h"
#include "workspace.h"

int main() {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      "bitmap16dx-workspace-regression";
  std::error_code error;
  std::filesystem::remove_all(root, error);
#ifdef _WIN32
  _putenv_s("BITMAP16_DATA_DIR", root.string().c_str());
#else
  setenv("BITMAP16_DATA_DIR", root.string().c_str(), 1);
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
  first.settings().defaultGridSize = 16;
  first.settings().matrixEnabled = true;
  if (!first.saveSettings()) return 3;

  bitmap16::Editor restoredEditor;
  bitmap16::Desktop::Workspace restored;
  if (!restored.initialize(restoredEditor)) return 4;
  if (restored.sketches().size() != 1 ||
      restoredEditor.sketch().pixels[2][3] != 4 ||
      restored.settings().theme != bitmap16::ThemeId::Dark ||
      restored.settings().defaultGridSize != 16 ||
      !restored.settings().matrixEnabled) {
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

  if (!restored.deleteSketch(0, restoredEditor) ||
      restored.sketches().size() != 1 ||
      !restored.undoDelete(restoredEditor) ||
      restored.sketches().size() != 2) {
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

  if (std::getenv("BITMAP16_KEEP_TEST_DATA") == nullptr) {
    std::filesystem::remove_all(root, error);
  }
  return 0;
}
#include <fstream>
