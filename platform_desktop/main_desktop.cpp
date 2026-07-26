#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#if defined(BITMAP16_CARDPUTER_ZERO_DEVICE) || defined(BITMAP16_STEAM_DECK)
#include <dirent.h>
#include <fstream>
#include <string>
#endif
#include <vector>

#include "core/canvas.h"
#include "core/canvas_view.h"
#include "core/charging_view.h"
#include "core/editor.h"
#include "core/help_view.h"
#include "core/memory_view.h"
#include "core/palette.h"
#include "core/palette_view.h"
#include "core/preview_view.h"
#include "core/settings_view.h"
#include "icons.h"
#include "cartridge_graphic.h"
#include "matrix_simulator.h"
#include "palettes.h"
#include "workspace.h"

#ifndef BITMAP16_DEFAULT_WIDTH
#define BITMAP16_DEFAULT_WIDTH 320
#endif

#ifndef BITMAP16_DEFAULT_HEIGHT
#define BITMAP16_DEFAULT_HEIGHT 170
#endif

namespace {

enum class DesktopView {
  Canvas,
  Help,
  Settings,
  Preview,
  Charging,
  Palette,
  Memory,
};

bitmap16::Desktop::MatrixSimulator* matrixSimulator = nullptr;
const bitmap16::Sketch* previewOverride = nullptr;
bool desktopMoveModeActive = false;
bool desktopDrawPressed = false;
bool desktopErasePressed = false;
bool desktopFillPressed = false;
bitmap16::CanvasView::Viewport desktopCanvasViewport;
char desktopStatus[32] = {};
Uint32 desktopStatusUntil = 0;

void setDesktopStatus(const char* message) {
  std::strncpy(desktopStatus, message, sizeof(desktopStatus) - 1);
  desktopStatus[sizeof(desktopStatus) - 1] = '\0';
  desktopStatusUntil = SDL_GetTicks() + 2000u;
}

int platformBatteryPercent() {
#ifdef BITMAP16_STEAM_DECK
  return -1;
#elif defined(BITMAP16_CARDPUTER_ZERO_DEVICE)
  constexpr const char* kPowerSupplyRoot = "/sys/class/power_supply";
  DIR* directory = opendir(kPowerSupplyRoot);
  if (directory == nullptr) return -1;
  int result = -1;
  while (const dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    const std::string path =
        std::string(kPowerSupplyRoot) + "/" + entry->d_name + "/capacity";
    std::ifstream capacity(path);
    int value = -1;
    if (capacity >> value && value >= 0 && value <= 100) {
      result = value;
      break;
    }
  }
  closedir(directory);
  return result;
#else
  return 78;
#endif
}

bool platformHasLedMatrixControls() {
#ifdef BITMAP16_STEAM_DECK
  return false;
#else
  return true;
#endif
}

bool platformHasShakeUndo() {
#ifdef BITMAP16_STEAM_DECK
  return false;
#else
  return true;
#endif
}

bool platformHasBatteryDisplay() {
#ifdef BITMAP16_STEAM_DECK
  return false;
#else
  return true;
#endif
}

struct AxisRepeat {
  int direction = 0;
  Uint32 nextAt = 0;
};

bool axisRepeatReady(
    AxisRepeat& state,
    int direction,
    Uint32 now,
    bool& repeat) {
  repeat = false;
  if (direction == 0) {
    state = {};
    return false;
  }
  if (direction != state.direction) {
    state.direction = direction;
    state.nextAt = now + 260u;
    return true;
  }
  if (now < state.nextAt) {
    return false;
  }
  state.nextAt = now + 90u;
  repeat = true;
  return true;
}

int stickDirection(Sint16 value) {
  constexpr Sint16 kDeadZone = 16000;
  if (value < -kDeadZone) return -1;
  if (value > kDeadZone) return 1;
  return 0;
}

void pushKeyEvent(SDL_Keycode key, bool repeat) {
  SDL_Event event = {};
  event.type = SDL_KEYDOWN;
  event.key.type = SDL_KEYDOWN;
  event.key.state = SDL_PRESSED;
  event.key.repeat = repeat ? 1 : 0;
  event.key.keysym.sym = key;
  event.key.keysym.mod = KMOD_NONE;
  SDL_PushEvent(&event);
}

enum DeckControllerEvent : Sint32 {
  kColorLeft = 1,
  kColorRight,
  kColorUp,
  kColorDown,
  kRedo,
};

void pushControllerEvent(DeckControllerEvent controllerEvent) {
  SDL_Event event = {};
  event.type = SDL_USEREVENT;
  event.user.type = SDL_USEREVENT;
  event.user.code = controllerEvent;
  SDL_PushEvent(&event);
}

const uint8_t* batteryIconForPercent(int batteryPercent) {
  if (batteryPercent < 10) return ICON_BATTERY_0;
  if (batteryPercent < 50) return ICON_BATTERY_10;
  if (batteryPercent < 90) return ICON_BATTERY_50;
  return ICON_BATTERY_90;
}

bool parseSize(const char* argument, int& width, int& height) {
  if (argument == nullptr) {
    return false;
  }
  char* end = nullptr;
  const long parsedWidth = std::strtol(argument, &end, 10);
  if (end == argument || end == nullptr || *end != 'x') {
    return false;
  }
  char* heightEnd = nullptr;
  const long parsedHeight = std::strtol(end + 1, &heightEnd, 10);
  if (heightEnd == end + 1 || *heightEnd != '\0' ||
      parsedWidth <= 0 || parsedHeight <= 0) {
    return false;
  }
  width = static_cast<int>(parsedWidth);
  height = static_cast<int>(parsedHeight);
  return true;
}

bitmap16::HelpView::Theme helpTheme(const bitmap16::Settings& settings) {
  if (settings.theme == bitmap16::ThemeId::Dark) {
    return {0x0861, 0xffff, 0x94b3};
  }
  return {0xd69b, 0x0000, 0x94b3};
}

bitmap16::SettingsView::Theme settingsTheme(
    const bitmap16::Settings& settings) {
  const bitmap16::HelpView::Theme theme = helpTheme(settings);
  return {theme.background, theme.text, theme.textSecondary};
}

bitmap16::PreviewView::Theme previewTheme() {
  return {0x0000, 0xffff, 0xd69b, 0x0861};
}

bitmap16::ChargingView::Theme chargingTheme() {
  return {0x0000, 0x0000, 0xd69b, 0xffff};
}

bitmap16::PaletteView::Theme paletteTheme(
    const bitmap16::Settings& settings) {
  const bitmap16::HelpView::Theme theme = helpTheme(settings);
  return {
      theme.background,
      theme.text,
      theme.textSecondary,
      settings.theme == bitmap16::ThemeId::Dark,
  };
}

bitmap16::MemoryView::Theme memoryTheme(
    const bitmap16::Settings& settings) {
  const bitmap16::HelpView::Theme theme = helpTheme(settings);
  return {
      theme.background,
      static_cast<uint16_t>(
          settings.theme == bitmap16::ThemeId::Dark ? 0x10a2 : 0xef7e),
      theme.text,
      0x0000,
      static_cast<uint16_t>(
          settings.theme == bitmap16::ThemeId::Dark ? 0xd69b : 0xffff),
      0xffe0,
  };
}

bitmap16::CanvasView::Theme canvasTheme(
    const bitmap16::Settings& settings) {
  if (settings.theme == bitmap16::ThemeId::Dark) {
    return {
        0x0861, 0x10a2, 0x2965, 0x0020, 0xffff, 0x94b3, 0x0861,
        0x0000, 0xd69b, true};
  }
  return {
      0xd69b, 0xef7e, 0xffff, 0xc63a, 0x0000, 0x94b3, 0xd69b,
      0x0000, 0xffff, false};
}

const bitmap16::CanvasView::Assets& canvasAssets() {
  static const bitmap16::CanvasView::Assets assets = {
      {ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT},
      {ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT},
      {ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT},
      {
          {ICON_BATTERY_0, 24, 24},
          {ICON_BATTERY_10, 24, 24},
          {ICON_BATTERY_50, 24, 24},
          {ICON_BATTERY_90, 24, 24},
      },
      {ICON_CANVAS_CURSOR,
       ICON_CANVAS_CURSOR_WIDTH,
       ICON_CANVAS_CURSOR_HEIGHT},
      {ICON_MOVE_CURSOR, ICON_MOVE_CURSOR_WIDTH, ICON_MOVE_CURSOR_HEIGHT},
      CURSOR_OFFSET_X,
      CURSOR_OFFSET_Y,
      MOVE_CURSOR_OFFSET_X,
      MOVE_CURSOR_OFFSET_Y,
  };
  return assets;
}

void makePaletteCatalogEntries(
    const bitmap16::Desktop::Workspace& workspace,
    std::vector<bitmap16::PaletteView::Entry>& entries) {
  entries.clear();
  entries.reserve(NUM_PALETTES + workspace.userPalettes().size());
  for (int palette = 0; palette < NUM_PALETTES; ++palette) {
    entries.push_back({
        PALETTE_CATALOG[palette],
        PALETTE_NAMES[palette],
        PALETTE_SIZES[palette],
        false,
    });
  }
  for (const auto& palette : workspace.userPalettes()) {
    entries.push_back({
        palette.colors.data(),
        palette.name.c_str(),
        palette.size,
        true,
    });
  }
}

void rebuildMemoryEntries(
    const bitmap16::Desktop::Workspace& workspace,
    std::vector<bitmap16::MemoryView::Entry>& entries) {
  entries.clear();
  entries.reserve(workspace.sketches().size());
  for (std::size_t index = 0;
       index < workspace.sketches().size();
       ++index) {
    const bitmap16::Sketch& sketch = workspace.sketches()[index];
    entries.push_back({
        sketch.pixels,
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
        static_cast<int>(index) == workspace.activeIndex(),
    });
  }
}

void applyPalette(
    bitmap16::Editor& editor,
    const bitmap16::PaletteView::Entry& entry) {
  if (entry.colors == nullptr ||
      (entry.size != 4 && entry.size != 8 && entry.size != 16)) {
    return;
  }
  editor.saveUndo();
  bitmap16::Sketch& sketch = editor.sketch();
  sketch.paletteSize = entry.size;
  for (int index = 0; index < 16; ++index) {
    sketch.paletteColors[index] = entry.colors[index];
  }
  for (std::size_t y = 0; y < bitmap16::kMaxGridSize; ++y) {
    for (std::size_t x = 0; x < bitmap16::kMaxGridSize; ++x) {
      sketch.pixels[y][x] =
          bitmap16::Palette::collapseIndex(
              sketch.pixels[y][x], entry.size);
    }
  }
}

int findActivePalette(
    const bitmap16::Sketch& sketch,
    const std::vector<bitmap16::PaletteView::Entry>& entries) {
  for (std::size_t palette = 0; palette < entries.size(); ++palette) {
    if (entries[palette].colors == nullptr ||
        entries[palette].size != sketch.paletteSize) {
      continue;
    }
    bool matches = true;
    for (int color = 0; color < 16; ++color) {
      if (entries[palette].colors[color] != sketch.paletteColors[color]) {
        matches = false;
        break;
      }
    }
    if (matches) return static_cast<int>(palette);
  }
  return -1;
}

void present(
    bitmap16::Canvas& canvas,
    SDL_Texture* texture,
    SDL_Renderer* renderer,
    uint8_t brightness) {
  SDL_UpdateTexture(
      texture,
      nullptr,
      canvas.pixels(),
      canvas.width() * static_cast<int>(sizeof(uint16_t)));
  const uint16_t background = canvas.readPixel(0, 0);
  const int brightnessScale =
      std::max(0, std::min(100, static_cast<int>(brightness)));
  const Uint8 backgroundRed = static_cast<Uint8>(
      (((background >> 11) & 0x1f) * 255 / 31) *
      brightnessScale / 100);
  const Uint8 backgroundGreen = static_cast<Uint8>(
      (((background >> 5) & 0x3f) * 255 / 63) *
      brightnessScale / 100);
  const Uint8 backgroundBlue = static_cast<Uint8>(
      ((background & 0x1f) * 255 / 31) *
      brightnessScale / 100);
  // SDL_RenderClear covers the full physical target rather than only the
  // logical viewport, so integer-scale margins inherit the active view.
  SDL_SetRenderDrawColor(
      renderer,
      backgroundRed,
      backgroundGreen,
      backgroundBlue,
      255);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  if (brightness < 100) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        renderer,
        0,
        0,
        0,
        static_cast<Uint8>((100 - brightness) * 255 / 100));
    SDL_RenderFillRect(renderer, nullptr);
  }
  SDL_RenderPresent(renderer);
}

void renderCurrentView(
    DesktopView view,
    bitmap16::Canvas& canvas,
    bitmap16::HelpView::State& helpState,
    bitmap16::SettingsView::State& settingsState,
    bitmap16::PreviewView::State& previewState,
    const bitmap16::PreviewView::Image&,
    bitmap16::ChargingView::State& chargingState,
    const bitmap16::ChargingView::SketchImage&,
    bitmap16::PaletteView::State& paletteState,
    const bitmap16::PaletteView::Catalog& paletteCatalog,
    int activePalette,
    bitmap16::MemoryView::State& memoryState,
    const bitmap16::MemoryView::Catalog& memoryCatalog,
    bitmap16::Editor& editor,
    bool rulersVisible,
    const bitmap16::Settings& settings,
    SDL_Texture* texture,
    SDL_Renderer* renderer) {
  if (view == DesktopView::Canvas) {
    const bitmap16::Sketch& sketch = editor.sketch();
    const bitmap16::CanvasView::State state = {
        sketch.pixels,
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
        editor.cursorX(),
        editor.cursorY(),
        editor.selectedColor(),
        rulersVisible,
        desktopMoveModeActive,
        desktopStatus[0] == '\0' ? nullptr : desktopStatus,
        platformBatteryPercent(),
        desktopDrawPressed,
        desktopErasePressed,
        desktopFillPressed,
        desktopCanvasViewport.cellSize,
        desktopCanvasViewport.x,
        desktopCanvasViewport.y,
#ifdef BITMAP16_STEAM_DECK
        true,
#else
        false,
#endif
    };
    bitmap16::CanvasView::render(
        canvas, state, canvasTheme(settings), &canvasAssets());
  } else if (view == DesktopView::Help) {
    bitmap16::HelpView::render(
        canvas,
        helpState,
        helpTheme(settings),
        platformHasLedMatrixControls(),
        platformHasBatteryDisplay());
  } else if (view == DesktopView::Settings) {
    bitmap16::SettingsView::render(
        canvas,
        settingsState,
        settings,
        settingsTheme(settings),
        false,
        platformHasLedMatrixControls(),
        platformHasShakeUndo());
  } else if (view == DesktopView::Preview) {
    const bitmap16::Sketch& sketch =
        previewOverride == nullptr ? editor.sketch() : *previewOverride;
    const bitmap16::PreviewView::Image previewImage = {
        sketch.pixels,
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
    };
    bitmap16::PreviewView::render(
        canvas,
        previewState,
        previewImage,
        previewTheme(),
        desktopStatus[0] == '\0' ? nullptr : desktopStatus);
  } else if (view == DesktopView::Charging) {
    const bitmap16::Sketch& sketch = editor.sketch();
    const bitmap16::ChargingView::SketchImage chargingSketch = {
        sketch.pixels,
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
    };
    bitmap16::ChargingView::render(
        canvas, chargingState, chargingTheme(), &chargingSketch);
  } else if (view == DesktopView::Palette) {
    bitmap16::PaletteView::render(
        canvas,
        paletteState,
        paletteCatalog,
        activePalette,
        paletteTheme(settings),
        CARTRIDGE_GRAPHIC);
  } else {
    const bitmap16::MemoryView::Assets assets = {
        ICON_SELECTOR_CORNER,
        ICON_SELECTOR_CORNER_WIDTH,
        ICON_SELECTOR_CORNER_HEIGHT,
    };
    bitmap16::MemoryView::render(
        canvas,
        memoryState,
        memoryCatalog,
        memoryTheme(settings),
        nullptr,
        &assets);
  }
  present(canvas, texture, renderer, settings.displayBrightness);
  if (matrixSimulator != nullptr) {
    const bitmap16::Sketch& matrixSketch =
        view == DesktopView::Preview && previewOverride != nullptr
            ? *previewOverride
            : editor.sketch();
    matrixSimulator->render(
        matrixSketch,
        settings,
        view == DesktopView::Canvas,
        editor.cursorX(),
        editor.cursorY());
  }
}

}  // namespace

int main(int argc, char** argv) {
  int width = BITMAP16_DEFAULT_WIDTH;
  int height = BITMAP16_DEFAULT_HEIGHT;
  bool smokeTest = false;
  for (int argument = 1; argument < argc; ++argument) {
    if (std::strcmp(argv[argument], "--smoke-test") == 0) {
      smokeTest = true;
    } else if (!parseSize(argv[argument], width, height)) {
      SDL_Log(
          "Usage: bitmap16dx_desktop [WIDTHxHEIGHT] [--smoke-test]");
      return 2;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }
  // Stick state is polled once per loop to provide controlled hold-repeat.
  // Per-motion events are redundant and can flood the queue on Steam Deck,
  // delaying navigation behind stale analog samples.
  SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_IGNORE);
  SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);

#if defined(BITMAP16_CARDPUTER_ZERO_DEVICE) || defined(BITMAP16_STEAM_DECK)
  const int windowWidth = width;
  const int windowHeight = height;
  const Uint32 windowFlags =
      SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS |
      SDL_WINDOW_FULLSCREEN_DESKTOP;
  SDL_ShowCursor(SDL_DISABLE);
#else
  const int windowWidth = width * 3;
  const int windowHeight = height * 3;
  const Uint32 windowFlags = SDL_WINDOW_SHOWN;
#endif

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  SDL_Window* window = SDL_CreateWindow(
#ifdef BITMAP16_STEAM_DECK
      "BitMap16 DX Steam Deck",
#else
      "BitMap16 DX Desktop",
#endif
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      windowWidth,
      windowHeight,
      windowFlags);
#ifdef BITMAP16_STEAM_DECK
  // The Deck presents its virtual keyboard when an application advertises
  // text entry. Bitmap16 uses key commands only and never needs text input.
  SDL_StopTextInput();
#endif
  SDL_Renderer* renderer = window == nullptr
      ? nullptr
      : SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_Texture* texture = renderer == nullptr
      ? nullptr
      : SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            width,
            height);
  if (renderer != nullptr) {
    SDL_RenderSetLogicalSize(renderer, width, height);
#ifdef BITMAP16_STEAM_DECK
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
#endif
  }

  bitmap16::Canvas canvas;
  if (window == nullptr || renderer == nullptr || texture == nullptr ||
      !canvas.create(width, height)) {
    SDL_Log("Desktop display creation failed: %s", SDL_GetError());
    if (texture != nullptr) SDL_DestroyTexture(texture);
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_GameController* controller = nullptr;
  const auto openController = [&](int deviceIndex) {
    if (controller != nullptr || !SDL_IsGameController(deviceIndex)) {
      return;
    }
    controller = SDL_GameControllerOpen(deviceIndex);
    if (controller != nullptr) {
      SDL_Log(
          "Controller: %s",
          SDL_GameControllerName(controller));
    } else {
      SDL_Log("Controller open failed: %s", SDL_GetError());
    }
  };
  for (int device = 0; device < SDL_NumJoysticks(); ++device) {
    openController(device);
  }

  DesktopView currentView = DesktopView::Canvas;
  DesktopView returnView = DesktopView::Canvas;
  bitmap16::HelpView::State helpState;
  bitmap16::SettingsView::State settingsState;
  bitmap16::PreviewView::State previewState;
  bitmap16::Editor editor;
  bitmap16::Desktop::Workspace workspace;
  if (!workspace.initialize(editor)) {
    SDL_Log("Workspace initialization failed");
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }
  SDL_Log("Workspace: %s", workspace.root().string().c_str());
  bitmap16::Settings& settings = workspace.settings();
  const bitmap16::Sketch& initialSketch = editor.sketch();
  const bitmap16::PreviewView::Image previewImage = {
      initialSketch.pixels,
      initialSketch.gridSize,
      initialSketch.paletteColors,
      initialSketch.paletteSize,
  };
  const int initialBatteryPercent = std::max(0, platformBatteryPercent());
  const uint8_t* const chargingIconPointers[4] = {
      ICON_DRAW,
      ICON_ERASE,
      ICON_FILL,
      batteryIconForPercent(initialBatteryPercent),
  };
  bitmap16::ChargingView::State chargingState;
  bitmap16::ChargingView::initialize(
      chargingState,
      width,
      height,
      42,
      chargingIconPointers,
      initialBatteryPercent,
      true);
  const bitmap16::ChargingView::SketchImage chargingSketch = {
      initialSketch.pixels,
      initialSketch.gridSize,
      initialSketch.paletteColors,
      initialSketch.paletteSize,
  };
  std::vector<bitmap16::PaletteView::Entry> paletteEntries;
  makePaletteCatalogEntries(workspace, paletteEntries);
  bitmap16::PaletteView::Catalog paletteCatalog = {
      paletteEntries.data(),
      static_cast<int>(paletteEntries.size()),
  };
  bitmap16::PaletteView::State paletteState;
  bitmap16::PaletteView::reset(paletteState, paletteCatalog);
  int activePalette = findActivePalette(editor.sketch(), paletteEntries);
  if (activePalette >= 0) {
    bitmap16::PaletteView::selectCatalogIndex(
        paletteState, activePalette);
  }
  std::vector<bitmap16::MemoryView::Entry> memoryEntries;
  rebuildMemoryEntries(workspace, memoryEntries);
  bitmap16::MemoryView::Catalog memoryCatalog = {
      memoryEntries.empty() ? nullptr : memoryEntries.data(),
      static_cast<int>(memoryEntries.size()),
  };
  bitmap16::MemoryView::State memoryState;
  const auto refreshMemoryCatalog = [&]() {
    rebuildMemoryEntries(workspace, memoryEntries);
    memoryCatalog.entries =
        memoryEntries.empty() ? nullptr : memoryEntries.data();
    memoryCatalog.count = static_cast<int>(memoryEntries.size());
    bitmap16::MemoryView::clamp(memoryState, memoryCatalog.count);
  };
  const auto refreshPaletteCatalog = [&]() {
    workspace.reloadUserPalettes();
    makePaletteCatalogEntries(workspace, paletteEntries);
    paletteCatalog.entries =
        paletteEntries.empty() ? nullptr : paletteEntries.data();
    paletteCatalog.count = static_cast<int>(paletteEntries.size());
    bitmap16::PaletteView::reset(paletteState, paletteCatalog);
    activePalette = findActivePalette(editor.sketch(), paletteEntries);
    if (activePalette >= 0) {
      bitmap16::PaletteView::selectCatalogIndex(
          paletteState, activePalette);
    }
  };
#if !defined(BITMAP16_CARDPUTER_ZERO_DEVICE) && !defined(BITMAP16_STEAM_DECK)
  bitmap16::Desktop::MatrixSimulator desktopMatrix;
  if (desktopMatrix.create()) {
    desktopMatrix.setEnabled(settings.matrixEnabled);
    matrixSimulator = &desktopMatrix;
  } else {
    SDL_Log("RGB matrix simulator creation failed: %s", SDL_GetError());
  }
#endif
  bool galleryMode = false;
  bool galleryAutoAdvance = false;
  int galleryIndex = 0;
  Uint32 galleryLastAdvance = SDL_GetTicks();
  Uint32 paletteCompletionAt = 0;
  bool rulersVisible = false;
  renderCurrentView(
      currentView,
      canvas,
      helpState,
      settingsState,
      previewState,
      previewImage,
      chargingState,
      chargingSketch,
      paletteState,
      paletteCatalog,
      activePalette,
      memoryState,
      memoryCatalog,
      editor,
      rulersVisible,
      settings,
      texture,
      renderer);

  if (smokeTest) {
    currentView = DesktopView::Settings;
    settingsState.cursor =
        bitmap16::SettingsView::itemCount(
            false,
            platformHasLedMatrixControls(),
            platformHasShakeUndo()) - 1;
    bitmap16::SettingsView::activate(
        settingsState,
        settings,
        false,
        platformHasLedMatrixControls(),
        platformHasShakeUndo());
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
    currentView = DesktopView::Preview;
    bitmap16::PreviewView::selectBackground(previewState, 2);
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
    currentView = DesktopView::Charging;
    bitmap16::ChargingView::update(chargingState, width, height);
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
    currentView = DesktopView::Palette;
    bitmap16::PaletteView::moveCursor(paletteState, 1);
    bitmap16::PaletteView::advance(paletteState, 1.0f);
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
    currentView = DesktopView::Memory;
    bitmap16::MemoryView::moveCursor(
        memoryState, 0, 1, memoryCatalog.count, width);
    bitmap16::MemoryView::advance(
        memoryState, memoryCatalog.count, width, height, 0.016f);
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
#if !defined(BITMAP16_CARDPUTER_ZERO_DEVICE) && !defined(BITMAP16_STEAM_DECK)
    if (matrixSimulator != nullptr) {
      matrixSimulator->setEnabled(true);
      settings.matrixUnits = 4;
      settings.matrixRotation = 1;
      settings.matrixBrightness = 20;
      currentView = DesktopView::Canvas;
      desktopMoveModeActive = true;
      renderCurrentView(
          currentView,
          canvas,
          helpState,
          settingsState,
          previewState,
          previewImage,
          chargingState,
          chargingSketch,
          paletteState,
          paletteCatalog,
          activePalette,
          memoryState,
          memoryCatalog,
          editor,
          rulersVisible,
          settings,
          texture,
          renderer);
      desktopMoveModeActive = false;
    }
#endif
  }

  const auto renderNow = [&]() {
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteState,
        paletteCatalog,
        activePalette,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer);
  };
  Uint32 memoryAnimationAt = SDL_GetTicks();
  const auto advanceMemoryAnimation = [&]() {
    const Uint32 now = SDL_GetTicks();
    if (currentView != DesktopView::Memory) {
      memoryAnimationAt = now;
      return;
    }
    const Uint32 elapsed = now - memoryAnimationAt;
    if (elapsed < 16u) return;
    memoryAnimationAt = now;
    bitmap16::MemoryView::advance(
        memoryState,
        memoryCatalog.count,
        width,
        height,
        static_cast<float>(std::min<Uint32>(elapsed, 100u)) / 1000.0f,
#ifdef BITMAP16_STEAM_DECK
        1.0f
#else
        0.35f
#endif
    );
    renderNow();
  };
  const auto advancePaletteAnimation = [&]() {
    if (currentView != DesktopView::Palette) return;
    const bitmap16::PaletteView::AnimationResult result =
        bitmap16::PaletteView::advance(paletteState);
    if (result ==
        bitmap16::PaletteView::AnimationResult::SelectionComplete) {
      if (paletteCompletionAt == 0) {
        paletteCompletionAt = SDL_GetTicks();
        renderNow();
      } else if (SDL_GetTicks() - paletteCompletionAt >= 500u) {
        paletteState.insertionAnimating = false;
        paletteCompletionAt = 0;
        currentView = DesktopView::Canvas;
        renderNow();
      }
    } else if (result != bitmap16::PaletteView::AnimationResult::Idle) {
      renderNow();
    }
  };
  const auto selectGallerySketch = [&]() {
    if (galleryMode && !workspace.sketches().empty()) {
      galleryIndex = std::max(
          0,
          std::min(
              static_cast<int>(workspace.sketches().size()) - 1,
              galleryIndex));
      previewOverride = &workspace.sketches()[galleryIndex];
    } else {
      previewOverride = nullptr;
    }
  };
  const auto adjustDisplayBrightness = [&](int delta) {
    settings.displayBrightness = static_cast<uint8_t>(
        std::max(
            10,
            std::min(
                100,
                static_cast<int>(settings.displayBrightness) + delta)));
    workspace.saveSettings();
    SDL_SetWindowBrightness(
        window, static_cast<float>(settings.displayBrightness) / 100.0f);
  };
  SDL_SetWindowBrightness(
      window, static_cast<float>(settings.displayBrightness) / 100.0f);

  const auto moveColorCursor = [&](int dx, int dy) {
    const int colorCount = editor.sketch().paletteSize;
    if (colorCount <= 0) return false;
    const int oldIndex = editor.selectedColor() - 1;
    const int rows = std::min(8, colorCount);
    int column = colorCount > 8 ? oldIndex / 8 : 0;
    int row = oldIndex % 8;
    if (dx != 0 && colorCount > 8) {
      column = std::max(0, std::min(1, column + dx));
    }
    if (dy != 0) {
      row = std::max(0, std::min(rows - 1, row + dy));
    }
    const int nextIndex =
        std::min(colorCount - 1, column * 8 + row);
    if (nextIndex == oldIndex) return false;
    editor.setSelectedColor(static_cast<uint8_t>(nextIndex + 1));
    return true;
  };

  AxisRepeat leftStickX;
  AxisRepeat leftStickY;
  AxisRepeat rightStickX;
  AxisRepeat rightStickY;
  AxisRepeat dpadX;
  AxisRepeat dpadY;
  bool controllerMoveHeld = false;
  bool controllerShiftStarted = false;
  bool controllerSaveChordLatched = false;
  bool previewLeftTriggerLatched = false;
  bool previewRightTriggerLatched = false;
  bool running = !smokeTest;
  while (running) {
    SDL_PumpEvents();
    if (desktopStatus[0] != '\0' &&
        SDL_TICKS_PASSED(SDL_GetTicks(), desktopStatusUntil)) {
      desktopStatus[0] = '\0';
      desktopStatusUntil = 0;
      if (currentView == DesktopView::Canvas ||
          currentView == DesktopView::Preview) {
        renderNow();
      }
    }
    const Uint8* heldKeys = SDL_GetKeyboardState(nullptr);
    const bool drawPressed =
        heldKeys[SDL_SCANCODE_RETURN] != 0 ||
        heldKeys[SDL_SCANCODE_SPACE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_A) != 0);
    const bool erasePressed =
        heldKeys[SDL_SCANCODE_BACKSPACE] != 0 ||
        heldKeys[SDL_SCANCODE_DELETE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_X) != 0);
    const bool fillPressed =
        heldKeys[SDL_SCANCODE_F] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_Y) != 0);
    const bool toolStateChanged =
        drawPressed != desktopDrawPressed ||
        erasePressed != desktopErasePressed ||
        fillPressed != desktopFillPressed;
    desktopDrawPressed = drawPressed;
    desktopErasePressed = erasePressed;
    desktopFillPressed = fillPressed;
    if (toolStateChanged && currentView == DesktopView::Canvas) {
      renderNow();
    }

    if (controller != nullptr) {
      const bool leftShoulderHeld =
          SDL_GameControllerGetButton(
              controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
      const bool leftTriggerHeld =
          SDL_GameControllerGetAxis(
              controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000;
      const bool rightTriggerHeld =
          SDL_GameControllerGetAxis(
              controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
      if (currentView == DesktopView::Preview) {
        const bitmap16::Sketch& previewSketch =
            previewOverride == nullptr
                ? editor.sketch()
                : *previewOverride;
        bool zoomChanged = false;
        if (leftTriggerHeld && !previewLeftTriggerLatched) {
          zoomChanged = bitmap16::PreviewView::adjustZoom(
              previewState,
              -1,
              previewSketch.gridSize,
              std::min(width, height));
        }
        if (rightTriggerHeld && !previewRightTriggerLatched) {
          zoomChanged =
              bitmap16::PreviewView::adjustZoom(
                  previewState,
                  1,
                  previewSketch.gridSize,
                  std::min(width, height)) ||
              zoomChanged;
        }
        if (zoomChanged) {
          renderNow();
        }
      }
      previewLeftTriggerLatched = leftTriggerHeld;
      previewRightTriggerLatched = rightTriggerHeld;
      const bool saveChordHeld =
          currentView == DesktopView::Canvas &&
          leftTriggerHeld &&
          rightTriggerHeld;
      if (saveChordHeld && !controllerSaveChordLatched) {
        pushKeyEvent(SDLK_s, false);
      }
      controllerSaveChordLatched = saveChordHeld;
      const bool nextMoveHeld =
          currentView == DesktopView::Canvas &&
          leftTriggerHeld &&
          !rightTriggerHeld &&
          !leftShoulderHeld;
      if (nextMoveHeld != controllerMoveHeld) {
        controllerMoveHeld = nextMoveHeld;
        controllerShiftStarted = false;
        if (currentView == DesktopView::Canvas) {
          desktopMoveModeActive = controllerMoveHeld;
          renderNow();
        }
      }

      const Uint32 now = SDL_GetTicks();
      bool repeat = false;
      const int leftX = stickDirection(SDL_GameControllerGetAxis(
          controller, SDL_CONTROLLER_AXIS_LEFTX));
      if (axisRepeatReady(leftStickX, leftX, now, repeat)) {
        pushKeyEvent(leftX < 0 ? SDLK_LEFT : SDLK_RIGHT, repeat);
      }
      const int leftY = stickDirection(SDL_GameControllerGetAxis(
          controller, SDL_CONTROLLER_AXIS_LEFTY));
      if (axisRepeatReady(leftStickY, leftY, now, repeat)) {
        pushKeyEvent(leftY < 0 ? SDLK_UP : SDLK_DOWN, repeat);
      }
      const int dpadHorizontal =
          (SDL_GameControllerGetButton(
               controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0 ? 1 : 0) -
          (SDL_GameControllerGetButton(
               controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0 ? 1 : 0);
      if (axisRepeatReady(dpadX, dpadHorizontal, now, repeat)) {
        pushKeyEvent(
            dpadHorizontal < 0 ? SDLK_LEFT : SDLK_RIGHT, repeat);
      }
      const int dpadVertical =
          (SDL_GameControllerGetButton(
               controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0 ? 1 : 0) -
          (SDL_GameControllerGetButton(
               controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0 ? 1 : 0);
      if (axisRepeatReady(dpadY, dpadVertical, now, repeat)) {
        pushKeyEvent(dpadVertical < 0 ? SDLK_UP : SDLK_DOWN, repeat);
      }
      const int rightX = stickDirection(SDL_GameControllerGetAxis(
          controller, SDL_CONTROLLER_AXIS_RIGHTX));
      if (axisRepeatReady(rightStickX, rightX, now, repeat)) {
        pushControllerEvent(rightX < 0 ? kColorLeft : kColorRight);
      }
      const int rightY = stickDirection(SDL_GameControllerGetAxis(
          controller, SDL_CONTROLLER_AXIS_RIGHTY));
      if (axisRepeatReady(rightStickY, rightY, now, repeat)) {
        pushControllerEvent(rightY < 0 ? kColorUp : kColorDown);
      }
    }

    // Advance independently of the SDL queue. Controller events can keep that
    // queue busy continuously, so timeout-only animations otherwise freeze.
    advanceMemoryAnimation();
    advancePaletteAnimation();

    SDL_Event event;
    const bool animatedView =
        controller != nullptr ||
        currentView == DesktopView::Charging ||
        currentView == DesktopView::Palette ||
        currentView == DesktopView::Memory ||
        (currentView == DesktopView::Preview &&
         galleryMode && galleryAutoAdvance);
    const int animationWait =
        currentView == DesktopView::Charging
            ? 33
            : currentView == DesktopView::Memory ? 1 : 16;
    const int eventAvailable = animatedView
        ? SDL_WaitEventTimeout(&event, animationWait)
        : SDL_WaitEvent(&event);
    if (eventAvailable == 0) {
      if (currentView == DesktopView::Charging) {
        bitmap16::ChargingView::update(chargingState, width, height);
        renderNow();
      } else if (
          currentView == DesktopView::Preview &&
          galleryMode && galleryAutoAdvance &&
          SDL_GetTicks() - galleryLastAdvance >= 3000u &&
          !workspace.sketches().empty()) {
        galleryIndex =
            (galleryIndex + 1) %
            static_cast<int>(workspace.sketches().size());
        galleryLastAdvance = SDL_GetTicks();
        selectGallerySketch();
        renderNow();
      }
      continue;
    }

    if (event.type == SDL_QUIT) {
      running = false;
      continue;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(window)) {
      running = false;
      continue;
    }
#ifdef BITMAP16_STEAM_DECK
    if (event.type == SDL_WINDOWEVENT &&
        event.window.windowID == SDL_GetWindowID(window) &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
      SDL_StopTextInput();
    }
#endif
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE &&
        matrixSimulator != nullptr &&
        event.window.windowID == matrixSimulator->windowId()) {
      matrixSimulator->destroy();
      matrixSimulator = nullptr;
      continue;
    }
    if (event.type == SDL_WINDOWEVENT &&
        matrixSimulator != nullptr &&
        event.window.windowID == matrixSimulator->windowId() &&
        (event.window.event == SDL_WINDOWEVENT_EXPOSED ||
         event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
      const bitmap16::Sketch& matrixSketch =
          currentView == DesktopView::Preview &&
                  previewOverride != nullptr
              ? *previewOverride
              : editor.sketch();
      matrixSimulator->render(
          matrixSketch,
          settings,
          currentView == DesktopView::Canvas,
          editor.cursorX(),
          editor.cursorY());
      continue;
    }
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
      openController(event.cdevice.which);
      continue;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED &&
        controller != nullptr) {
      SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
      if (SDL_JoystickInstanceID(joystick) == event.cdevice.which) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
        controllerMoveHeld = false;
        controllerShiftStarted = false;
        controllerSaveChordLatched = false;
        previewLeftTriggerLatched = false;
        previewRightTriggerLatched = false;
        desktopMoveModeActive = false;
        leftStickX = {};
        leftStickY = {};
        rightStickX = {};
        rightStickY = {};
        dpadX = {};
        dpadY = {};
        renderNow();
      }
      continue;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
      if (currentView == DesktopView::Preview &&
          (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
           event.cbutton.button == SDL_CONTROLLER_BUTTON_X ||
           event.cbutton.button == SDL_CONTROLLER_BUTTON_Y)) {
        const int background =
            event.cbutton.button == SDL_CONTROLLER_BUTTON_X
                ? static_cast<int>(
                      bitmap16::PreviewView::Background::Black)
                : event.cbutton.button == SDL_CONTROLLER_BUTTON_Y
                    ? static_cast<int>(
                          bitmap16::PreviewView::Background::White)
                    : static_cast<int>(
                          bitmap16::PreviewView::Background::Gray);
        if (bitmap16::PreviewView::selectBackground(
                previewState, background)) {
          renderNow();
        }
        continue;
      }
      SDL_Keycode mappedKey = SDLK_UNKNOWN;
      switch (event.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_A:
          mappedKey = SDLK_RETURN;
          break;
        case SDL_CONTROLLER_BUTTON_B:
          mappedKey = SDLK_ESCAPE;
          break;
        case SDL_CONTROLLER_BUTTON_X:
          mappedKey = SDLK_BACKSPACE;
          break;
        case SDL_CONTROLLER_BUTTON_Y:
          mappedKey = SDLK_f;
          break;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
          mappedKey = SDLK_p;
          break;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
          mappedKey = SDLK_g;
          break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
          pushControllerEvent(kRedo);
          break;
        case SDL_CONTROLLER_BUTTON_BACK:
          mappedKey = SDLK_o;
          break;
        case SDL_CONTROLLER_BUTTON_START:
          mappedKey = SDLK_t;
          break;
#if defined(BITMAP16_STEAM_DECK) && SDL_VERSION_ATLEAST(2, 0, 14)
        // SDL numbers rear paddles while facing the back of the controller:
        // Paddle 2/4 are the Deck's upper/lower left grips (L4/L5), and
        // Paddle 1/3 are its upper/lower right grips (R4/R5).
        case SDL_CONTROLLER_BUTTON_PADDLE2:
          mappedKey = SDLK_g;
          break;
        case SDL_CONTROLLER_BUTTON_PADDLE4:
          mappedKey = SDLK_v;
          break;
        case SDL_CONTROLLER_BUTTON_PADDLE1:
          mappedKey = SDLK_h;
          break;
        case SDL_CONTROLLER_BUTTON_PADDLE3:
          mappedKey = SDLK_r;
          break;
#endif
        default:
          break;
      }
      if (mappedKey != SDLK_UNKNOWN) {
        pushKeyEvent(mappedKey, false);
      }
      continue;
    }
    if (event.type == SDL_CONTROLLERBUTTONUP &&
        event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
      pushKeyEvent(SDLK_z, false);
      continue;
    }
    if (event.type == SDL_USEREVENT && event.user.code == kRedo) {
      if (currentView == DesktopView::Canvas) {
        const bool redone = editor.redo();
        setDesktopStatus(redone ? "Redo" : "No redo");
        renderNow();
      }
      continue;
    }
    if (event.type == SDL_USEREVENT &&
        event.user.code >= kColorLeft &&
        event.user.code <= kColorDown) {
      bool changed = false;
      if (currentView == DesktopView::Canvas) {
        const int dx =
            event.user.code == kColorLeft
                ? -1
                : event.user.code == kColorRight ? 1 : 0;
        const int dy =
            event.user.code == kColorUp
                ? -1
                : event.user.code == kColorDown ? 1 : 0;
        changed = moveColorCursor(dx, dy);
      } else if (
          currentView == DesktopView::Palette &&
          (event.user.code == kColorLeft ||
           event.user.code == kColorRight)) {
        changed = bitmap16::PaletteView::moveCursor(
            paletteState,
            event.user.code == kColorLeft ? -1 : 1);
      }
      if (changed) renderNow();
      continue;
    }
    if (event.type == SDL_KEYUP &&
        event.key.keysym.sym == SDLK_m &&
        desktopMoveModeActive) {
      desktopMoveModeActive = false;
      renderNow();
      continue;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.windowID == SDL_GetWindowID(window) &&
        event.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
        desktopMoveModeActive) {
      desktopMoveModeActive = false;
      renderNow();
      continue;
    }
    if (event.type != SDL_KEYDOWN) continue;

    const SDL_Keycode key = event.key.keysym.sym;
    const bool arrow =
        key == SDLK_UP || key == SDLK_DOWN ||
        key == SDLK_LEFT || key == SDLK_RIGHT;
    if (event.key.repeat != 0 && !arrow) continue;
    const bool altHeld = (event.key.keysym.mod & KMOD_ALT) != 0;
    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
    const bool moveHeld =
        keyboard[SDL_SCANCODE_M] != 0 || controllerMoveHeld;
    const bool brightnessHeld = keyboard[SDL_SCANCODE_B] != 0;
    const bool matrixHeld = keyboard[SDL_SCANCODE_L] != 0;
    const bool drawHeld =
        keyboard[SDL_SCANCODE_RETURN] != 0 ||
        keyboard[SDL_SCANCODE_SPACE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_A) != 0);
    const bool eraseHeld =
        keyboard[SDL_SCANCODE_BACKSPACE] != 0 ||
        keyboard[SDL_SCANCODE_DELETE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_X) != 0);
    const bool plusKey =
        key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS;
    const bool minusKey = key == SDLK_MINUS || key == SDLK_KP_MINUS;
    bool changed = false;
    const auto moveCanvasCursor = [&](int dx, int dy) {
      if (moveHeld) {
        const bool saveUndo =
            controllerMoveHeld
                ? !controllerShiftStarted
                : event.key.repeat == 0;
        const bool shifted = editor.shift(dx, dy, saveUndo);
        if (shifted && controllerMoveHeld) {
          controllerShiftStarted = true;
        }
        return shifted;
      }
      const bool moved = editor.moveCursor(dx, dy);
      if (!moved) return false;
      bitmap16::CanvasView::keepCursorVisible(
          desktopCanvasViewport,
          width,
          height,
          editor.sketch().gridSize,
          editor.cursorX(),
          editor.cursorY());
      if (drawHeld) editor.draw();
      if (eraseHeld) editor.erase();
      return true;
    };

    if (key == SDLK_m && currentView == DesktopView::Canvas) {
      desktopMoveModeActive = true;
      changed = true;
    } else if (key == SDLK_ESCAPE) {
      if (currentView == DesktopView::Preview && galleryMode) {
        currentView = DesktopView::Memory;
        memoryState.cursor = galleryIndex + 1;
        galleryMode = false;
        galleryAutoAdvance = false;
        previewOverride = nullptr;
      } else if (currentView == DesktopView::Help) {
        currentView = returnView;
        previewOverride = nullptr;
      } else if (currentView != DesktopView::Canvas) {
        currentView = DesktopView::Canvas;
        previewOverride = nullptr;
      }
      changed = true;
    } else if (key == SDLK_q) {
      running = false;
    } else if (currentView == DesktopView::Charging) {
      currentView = DesktopView::Canvas;
      changed = true;
    } else if (brightnessHeld && (plusKey || minusKey)) {
      adjustDisplayBrightness(plusKey ? 5 : -5);
      changed = true;
    } else if (matrixHeld && (plusKey || minusKey)) {
      settings.matrixBrightness = static_cast<uint8_t>(
          std::max(
              1,
              std::min(
                  20,
                  static_cast<int>(settings.matrixBrightness) +
                      (plusKey ? 1 : -1))));
      workspace.saveSettings();
      changed = true;
    } else if (
        currentView == DesktopView::Canvas &&
        (plusKey || minusKey)) {
      changed = bitmap16::CanvasView::adjustZoom(
          desktopCanvasViewport,
          plusKey ? 1 : -1,
          width,
          height,
          editor.sketch().gridSize,
          editor.cursorX(),
          editor.cursorY());
    } else if (
        matrixHeld && (key == SDLK_RETURN || key == SDLK_SPACE) &&
        matrixSimulator != nullptr) {
      matrixSimulator->toggle();
      settings.matrixEnabled = matrixSimulator->isEnabled();
      workspace.saveSettings();
      changed = true;
    } else if (key == SDLK_h) {
      if (currentView == DesktopView::Help) {
        currentView = returnView;
      } else {
        returnView = currentView;
        currentView = DesktopView::Help;
      }
      changed = true;
    } else if (key == SDLK_t) {
      currentView = currentView == DesktopView::Settings
          ? DesktopView::Canvas
          : DesktopView::Settings;
      changed = true;
    } else if (key == SDLK_v) {
      if (currentView == DesktopView::Preview) {
        currentView = galleryMode
            ? DesktopView::Memory
            : DesktopView::Canvas;
        galleryMode = false;
        galleryAutoAdvance = false;
        previewOverride = nullptr;
      } else {
        bitmap16::PreviewView::selectBackground(
            previewState,
            static_cast<int>(
                settings.theme == bitmap16::ThemeId::Dark
                    ? bitmap16::PreviewView::Background::Dark
                    : bitmap16::PreviewView::Background::Gray));
        setDesktopStatus("Preview");
        galleryMode =
            currentView == DesktopView::Memory &&
            !workspace.sketches().empty();
        if (galleryMode) {
          galleryIndex = memoryState.cursor > 0
              ? memoryState.cursor - 1
              : 0;
          galleryAutoAdvance = false;
          galleryLastAdvance = SDL_GetTicks();
        }
        selectGallerySketch();
        currentView = DesktopView::Preview;
      }
      changed = true;
    } else if (
        key == SDLK_b && altHeld && platformHasBatteryDisplay()) {
      currentView = DesktopView::Charging;
      changed = true;
    } else if (key == SDLK_p) {
      if (currentView == DesktopView::Palette) {
        currentView = DesktopView::Canvas;
      } else {
        refreshPaletteCatalog();
        currentView = DesktopView::Palette;
      }
      changed = true;
    } else if (key == SDLK_o) {
      if (currentView == DesktopView::Memory) {
        currentView = DesktopView::Canvas;
      } else {
        refreshMemoryCatalog();
        currentView = DesktopView::Memory;
      }
      changed = true;
    } else if (key == SDLK_d) {
      currentView = DesktopView::Canvas;
      changed = true;
    } else if (currentView == DesktopView::Canvas && key == SDLK_s) {
      if (workspace.saveSketch(editor, altHeld)) {
        refreshMemoryCatalog();
        SDL_Log(altHeld ? "Saved new sketch" : "Saved sketch");
        setDesktopStatus("Saved");
        changed = true;
      } else {
        SDL_Log("Sketch save failed");
        setDesktopStatus("Failed to save");
        changed = true;
      }
    } else if (currentView == DesktopView::Canvas && key == SDLK_x) {
      std::filesystem::path outputPath;
      if (workspace.exportSketch(editor.sketch(), !altHeld, outputPath)) {
        SDL_Log("Exported %s", outputPath.string().c_str());
        SDL_SetWindowTitle(
            window,
            ("BitMap16 DX - Exported " + outputPath.filename().string())
                .c_str());
        setDesktopStatus("Exported!");
      } else {
        SDL_Log("PNG export failed");
        setDesktopStatus("Export failed");
      }
      changed = true;
    } else if (currentView == DesktopView::Canvas && key == SDLK_n) {
      workspace.newSketch(editor);
      desktopCanvasViewport = {};
      activePalette = findActivePalette(editor.sketch(), paletteEntries);
      setDesktopStatus("New sketch");
      changed = true;
    } else if (currentView == DesktopView::Canvas && key == SDLK_c) {
      const uint8_t next = static_cast<uint8_t>(
          editor.selectedColor() % editor.sketch().paletteSize + 1);
      editor.setSelectedColor(next);
      changed = true;
    } else if (currentView == DesktopView::Canvas && key == SDLK_f) {
      editor.floodFill();
      setDesktopStatus("Fill");
      changed = true;
    } else if (currentView == DesktopView::Canvas && key == SDLK_z) {
      const bool performed = altHeld ? editor.redo() : editor.undo();
      setDesktopStatus(
          altHeld
              ? performed ? "Redo" : "No redo"
              : performed ? "Undo" : "No undo");
      changed = true;
    } else if (
        currentView == DesktopView::Canvas && key == SDLK_y &&
        settings.shakeUndoEnabled) {
      changed = editor.undo();
    } else if (currentView == DesktopView::Canvas && key == SDLK_g) {
      changed = editor.toggleGridSize();
      desktopCanvasViewport = {};
      char gridStatus[16] = {};
      std::snprintf(
          gridStatus,
          sizeof(gridStatus),
          "%ux%u",
          editor.sketch().gridSize,
          editor.sketch().gridSize);
      setDesktopStatus(gridStatus);
    } else if (currentView == DesktopView::Canvas && key == SDLK_r) {
      rulersVisible = !rulersVisible;
      setDesktopStatus(rulersVisible ? "Rulers: On" : "Rulers: Off");
      changed = true;
    } else if (
        currentView == DesktopView::Canvas &&
        (key == SDLK_RETURN || key == SDLK_SPACE)) {
      changed = editor.draw();
    } else if (
        currentView == DesktopView::Canvas &&
        (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
      if (altHeld) {
        editor.clear();
        setDesktopStatus("Clear");
        changed = true;
      } else {
        changed = editor.erase();
      }
    } else if (currentView == DesktopView::Canvas && key == SDLK_k) {
      editor.clear();
      setDesktopStatus("Clear");
      changed = true;
    } else if (
        currentView == DesktopView::Canvas &&
        key >= SDLK_1 && key <= SDLK_8) {
      const uint8_t color = static_cast<uint8_t>(
          key - SDLK_0 + (altHeld ? 8 : 0));
      if (color <= editor.sketch().paletteSize) {
        editor.setSelectedColor(color);
        changed = true;
      }
    } else if (arrow && currentView == DesktopView::Preview && galleryMode) {
      if ((key == SDLK_LEFT || key == SDLK_RIGHT) &&
          !workspace.sketches().empty()) {
        const int count = static_cast<int>(workspace.sketches().size());
        galleryIndex =
            (galleryIndex + (key == SDLK_LEFT ? count - 1 : 1)) % count;
        galleryAutoAdvance = false;
        selectGallerySketch();
        changed = true;
      }
    } else if (key == SDLK_UP) {
      if (currentView == DesktopView::Canvas) {
        changed = moveCanvasCursor(0, -1);
      } else if (currentView == DesktopView::Help) {
        changed = bitmap16::HelpView::moveCursor(
            helpState,
            -1,
            platformHasLedMatrixControls(),
            platformHasBatteryDisplay());
      } else if (currentView == DesktopView::Settings) {
        changed = bitmap16::SettingsView::moveCursor(
            settingsState,
            -1,
            false,
            platformHasLedMatrixControls(),
            platformHasShakeUndo());
      } else if (currentView == DesktopView::Memory) {
        changed = bitmap16::MemoryView::moveCursor(
            memoryState, 0, -1, memoryCatalog.count, width);
      }
    } else if (key == SDLK_DOWN) {
      if (currentView == DesktopView::Canvas) {
        changed = moveCanvasCursor(0, 1);
      } else if (currentView == DesktopView::Help) {
        changed = bitmap16::HelpView::moveCursor(
            helpState,
            1,
            platformHasLedMatrixControls(),
            platformHasBatteryDisplay());
      } else if (currentView == DesktopView::Settings) {
        changed = bitmap16::SettingsView::moveCursor(
            settingsState,
            1,
            false,
            platformHasLedMatrixControls(),
            platformHasShakeUndo());
      } else if (currentView == DesktopView::Memory) {
        changed = bitmap16::MemoryView::moveCursor(
            memoryState, 0, 1, memoryCatalog.count, width);
      }
    } else if (
        currentView == DesktopView::Canvas &&
        (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      const int delta = key == SDLK_LEFT ? -1 : 1;
      changed = moveCanvasCursor(delta, 0);
    } else if (
        currentView == DesktopView::Settings &&
        (key == SDLK_LEFT || key == SDLK_RIGHT ||
         key == SDLK_RETURN || key == SDLK_SPACE)) {
      const bitmap16::SettingsView::Action action =
          bitmap16::SettingsView::activate(
              settingsState,
              settings,
              false,
              platformHasLedMatrixControls(),
              platformHasShakeUndo());
      changed = action != bitmap16::SettingsView::Action::None;
      if (changed) workspace.saveSettings();
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      changed = bitmap16::PaletteView::moveCursor(
          paletteState, key == SDLK_LEFT ? -1 : 1);
    } else if (
        currentView == DesktopView::Memory &&
        (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      changed = bitmap16::MemoryView::moveCursor(
          memoryState,
          key == SDLK_LEFT ? -1 : 1,
          0,
          memoryCatalog.count,
          width);
    } else if (
        currentView == DesktopView::Memory && key == SDLK_RETURN) {
      if (memoryState.cursor == 0) {
        workspace.newSketch(editor);
        desktopCanvasViewport = {};
        activePalette = findActivePalette(editor.sketch(), paletteEntries);
        currentView = DesktopView::Canvas;
        refreshMemoryCatalog();
        setDesktopStatus("New sketch");
        changed = true;
      } else if (workspace.openSketch(
                     static_cast<std::size_t>(memoryState.cursor - 1),
                     editor)) {
        desktopCanvasViewport = {};
        activePalette = findActivePalette(editor.sketch(), paletteEntries);
        currentView = DesktopView::Canvas;
        refreshMemoryCatalog();
        setDesktopStatus("Loaded");
        changed = true;
      }
    } else if (
        currentView == DesktopView::Memory &&
        (key == SDLK_BACKSPACE || key == SDLK_DELETE) &&
        memoryState.cursor > 0) {
      changed = workspace.deleteSketch(
          static_cast<std::size_t>(memoryState.cursor - 1), editor);
      if (changed) {
        refreshMemoryCatalog();
        activePalette = findActivePalette(editor.sketch(), paletteEntries);
        setDesktopStatus("Restored sketch");
      }
    } else if (
        currentView == DesktopView::Memory && key == SDLK_z) {
      changed = workspace.undoDelete(editor);
      if (changed) {
        refreshMemoryCatalog();
        activePalette = findActivePalette(editor.sketch(), paletteEntries);
      }
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
      if (bitmap16::PaletteView::beginSelection(paletteState)) {
        paletteCompletionAt = 0;
        activePalette =
            bitmap16::PaletteView::selectedCatalogIndex(paletteState);
        if (activePalette >= 0 &&
            activePalette < paletteCatalog.count) {
          applyPalette(editor, paletteEntries[activePalette]);
        }
        changed = true;
      }
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_0 || key == SDLK_4 ||
         key == SDLK_8 || key == SDLK_1)) {
      if (key == SDLK_0) {
        bitmap16::PaletteView::reset(paletteState, paletteCatalog);
      } else {
        const uint8_t size =
            key == SDLK_4 ? 4 : key == SDLK_8 ? 8 : 16;
        bitmap16::PaletteView::toggleSizeFilter(
            paletteState, paletteCatalog, size);
      }
      changed = true;
    } else if (
        currentView == DesktopView::Palette && key == SDLK_u) {
      bitmap16::PaletteView::toggleUserFilter(
          paletteState, paletteCatalog);
      changed = true;
    } else if (
        currentView == DesktopView::Preview &&
        key >= SDLK_1 && key <= SDLK_4) {
      changed = bitmap16::PreviewView::selectBackground(
          previewState, static_cast<int>(key - SDLK_1));
    } else if (
        currentView == DesktopView::Preview &&
        galleryMode && key == SDLK_SPACE) {
      galleryAutoAdvance = !galleryAutoAdvance;
      galleryLastAdvance = SDL_GetTicks();
      changed = true;
    }
    if (changed) renderNow();
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
#if !defined(BITMAP16_CARDPUTER_ZERO_DEVICE) && !defined(BITMAP16_STEAM_DECK)
  if (matrixSimulator != nullptr) {
    matrixSimulator->destroy();
    matrixSimulator = nullptr;
  }
#endif
  if (controller != nullptr) {
    SDL_GameControllerClose(controller);
    controller = nullptr;
  }
  SDL_Quit();
  return 0;
}
