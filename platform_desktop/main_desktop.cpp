#include "core/dialog_view.h"
#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <random>
#ifdef BITMAP16_LOSPEC
#include "lospec.h"
#endif
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
#include "core/sketch_workflow.h"
#include "core/help_view.h"
#include "core/memory_view.h"
#include "core/palette.h"
#include "core/palette_view.h"
#include "core/preview_view.h"
#include "core/settings_view.h"
#include "icons.h"
#include "screensaver_icons.h"
#include "cartridge_graphic.h"
#include "matrix_simulator.h"
#include "palettes.h"
#include "workspace.h"
#include "palette_browser.h"
#include "audio_player.h"
#include "menu_audio.h"

#ifdef BITMAP16_CARDPUTER_ZERO
#include "../platform_zero/keyboard_event.h"
#endif

#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
#include "../platform_zero/imu.h"
#include "../platform_zero/native_io.h"
#endif

#ifndef PROGMEM
#define PROGMEM
#endif
#include "../src/boot_image.h"

#ifndef BITMAP16_DEFAULT_WIDTH
#define BITMAP16_DEFAULT_WIDTH 320
#endif

#ifndef BITMAP16_DEFAULT_HEIGHT
#define BITMAP16_DEFAULT_HEIGHT 170
#endif

namespace {

#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
bitmap16::Zero::NativeIo nativeIo;
bool zeroHomeRequested = false;
bitmap16::Zero::Imu zeroImu;
#endif

const Uint8* keyboardState() {
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  return nativeIo.keyboardState();
#else
  return SDL_GetKeyboardState(nullptr);
#endif
}

#ifdef BITMAP16_CARDPUTER_ZERO
bool externalKeyboardActive() {
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  return nativeIo.externalKeyboardActive();
#else
  return true;
#endif
}
#endif

bitmap16::Desktop::AudioPlayer* appAudio = nullptr;
std::function<void(SDL_Keycode)> musicShortcut;
Uint32 musicEventType = static_cast<Uint32>(-1);

int waitEvent(SDL_Event* event, int timeout = -1) {
  const Uint32 started = SDL_GetTicks();
  while (true) {
    if (appAudio != nullptr) appAudio->update();
    const Uint32 elapsed = SDL_GetTicks() - started;
    int remaining = timeout < 0 ? -1 : std::max(0, timeout - static_cast<int>(elapsed));
    // Check track completion even on idle screens and inside modal dialogs.
    const int slice = appAudio != nullptr && appAudio->musicEnabled()
        ? remaining < 0 ? 100 : std::min(remaining, 100) : remaining;
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    const int available = nativeIo.waitEvent(event, slice);
    if (available && (nativeIo.isHomeEvent(*event) ||
                     (event->type == SDL_QUIT && nativeIo.escapeHeld()))) {
      zeroHomeRequested = true;
      *event = {};
      event->type = SDL_QUIT;
    }
#else
    const int available = slice < 0 ? SDL_WaitEvent(event) : SDL_WaitEventTimeout(event, slice);
#endif
    if (available) {
#ifdef BITMAP16_STEAM_DECK
      // Handle L3 here so music works in every screen and nested dialog.
      if (musicShortcut &&
          (event->type == SDL_CONTROLLERBUTTONDOWN || event->type == SDL_CONTROLLERBUTTONUP) &&
          event->cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK) {
        if (event->type == SDL_CONTROLLERBUTTONDOWN) {
          musicShortcut(SDLK_AUDIOPLAY);
          *event = {};
          event->type = musicEventType;
          return 1;
        }
        continue;
      }
#endif
      if (musicShortcut && (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) &&
          (event->key.keysym.sym == SDLK_AUDIOPLAY || event->key.keysym.sym == SDLK_AUDIOPREV ||
           event->key.keysym.sym == SDLK_AUDIONEXT)) {
        if (event->type == SDL_KEYDOWN && event->key.repeat == 0) {
          musicShortcut(event->key.keysym.sym);
          *event = {};
          event->type = musicEventType;
          return 1;
        }
        continue;
      }
      return 1;
    }
    if (timeout >= 0 && SDL_GetTicks() - started >= static_cast<Uint32>(timeout)) return 0;
  }
}

using DesktopView = bitmap16::Desktop::View;

bitmap16::Desktop::MatrixSimulator* matrixSimulator = nullptr;
const bitmap16::Sketch* previewOverride = nullptr;
bool desktopMoveModeActive = false;
bool desktopDrawPressed = false;
bool desktopErasePressed = false;
bool desktopFillPressed = false;
bool desktopPalettePressed = false;
#ifdef BITMAP16_STEAM_DECK
bool desktopControllerPrompts = true;
#endif
bitmap16::CanvasView::Viewport desktopCanvasViewport;
char desktopStatus[32] = {};
Uint32 desktopStatusUntil = 0;
uint64_t desktopPresentedFrames = 0;
Uint32 previewLabelUntil = 0;
char previewLabel[20] = {};

void setDesktopStatus(const char* message) {
  std::strncpy(desktopStatus, message, sizeof(desktopStatus) - 1);
  desktopStatus[sizeof(desktopStatus) - 1] = '\0';
  desktopStatusUntil = SDL_GetTicks() + 2000u;
}

void setPreviewLabel(const char* message) {
  std::strncpy(previewLabel, message, sizeof(previewLabel) - 1);
  previewLabel[sizeof(previewLabel) - 1] = '\0';
  previewLabelUntil = SDL_GetTicks() + 2000u;
}

struct BatteryReading {
  int percent = -1;
  bool charging = false;
};

BatteryReading readPlatformBattery() {
#ifdef BITMAP16_STEAM_DECK
  return {};
#elif defined(BITMAP16_CARDPUTER_ZERO_DEVICE)
  constexpr const char* kPowerSupplyRoot = "/sys/class/power_supply";
  DIR* directory = opendir(kPowerSupplyRoot);
  if (directory == nullptr) return {};
  BatteryReading result;
  while (const dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    const std::string path = std::string(kPowerSupplyRoot) + "/" + entry->d_name;
    std::ifstream capacity(path + "/capacity");
    int value = -1;
    if (capacity >> value && value >= 0 && value <= 100) {
      std::ifstream status(path + "/status");
      std::string statusText;
      std::getline(status, statusText);
      result = {value, statusText == "Charging"};
      break;
    }
  }
  closedir(directory);
  return result;
#else
  return {78, false};
#endif
}

BatteryReading batteryReading;

#ifdef BITMAP16_STEAM_DECK
SDL_Keycode paletteFilterKey(Uint8 button, uint8_t currentFilter, bool localList) {
  if (button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ||
      button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
    const auto next = bitmap16::PaletteView::steppedSizeFilter(currentFilter,
        button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ? -1 : 1);
    return next == 4 ? SDLK_4 : next == 8 ? SDLK_8 : next == 16 ? SDLK_1 : SDLK_0;
  }
  if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) return SDLK_0;
  if (button == SDL_CONTROLLER_BUTTON_DPAD_UP && localList) return SDLK_u;
  return SDLK_UNKNOWN;
}
#endif

bool platformHasLedMatrixControls() {
#if defined(BITMAP16_STEAM_DECK) || (defined(ENABLE_LED_MATRIX) && !ENABLE_LED_MATRIX)
  return false;
#else
  return true;
#endif
}

bool platformHasShakeUndo() {
#ifdef BITMAP16_STEAM_DECK
  return false;
#elif defined(BITMAP16_CARDPUTER_ZERO_DEVICE)
  return zeroImu.isAvailable();
#else
  return true;
#endif
}

bool platformHasQuitSetting() {
#ifdef BITMAP16_STEAM_DECK
  return true;
#else
  return false;
#endif
}

bool platformHasHelpSetting() {
#ifdef BITMAP16_STEAM_DECK
  return true;
#else
  return false;
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
  kToggleSettings,
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

constexpr uint16_t platformDarkBackground() {
#ifdef BITMAP16_STEAM_DECK
  return 0x10a2;
#else
  return 0x0861;
#endif
}

constexpr uint16_t platformDarkSurface() {
#ifdef BITMAP16_STEAM_DECK
  return 0x18e3;
#else
  return 0x10a2;
#endif
}

bitmap16::HelpView::Theme helpTheme(const bitmap16::Settings& settings) {
  if (settings.theme == bitmap16::ThemeId::Dark) {
    return {platformDarkBackground(), 0xffff, 0x94b3};
  }
  return {0xd69b, 0x0000, 0x94b3};
}

bitmap16::SettingsView::Theme settingsTheme(
    const bitmap16::Settings& settings) {
  const bitmap16::HelpView::Theme theme = helpTheme(settings);
  return {theme.background, theme.text, theme.textSecondary};
}

bitmap16::PreviewView::Theme previewTheme() {
  return {0x0000, 0xffff, 0xd69b, platformDarkBackground()};
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
          settings.theme == bitmap16::ThemeId::Dark
              ? platformDarkSurface()
              : 0xef7e),
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
        platformDarkBackground(), platformDarkSurface(), 0x2965, 0x0020,
        0xffff, 0x94b3, platformDarkBackground(),
        0x0000, 0xd69b, true};
  }
  return {
      0xd69b, 0xef7e, 0xffff, 0xc63a, 0x0000, 0x94b3, 0xd69b,
      0x0000, 0xffff, false};
}

int desktopResizeAction = -1;

bitmap16::DialogView::Theme dialogTheme(const bitmap16::Settings& settings) {
  const auto theme = canvasTheme(settings);
  return {theme.background, theme.text, theme.iconDark, theme.iconLight, theme.dark};
}

#ifdef BITMAP16_LOSPEC
bitmap16::DialogView::Frame drawDialogFrame(
    bitmap16::Canvas& canvas, const bitmap16::Settings& settings,
    const char* title, int height) {
  return bitmap16::DialogView::drawFrame(canvas, dialogTheme(settings), title, height);
}

void drawDialogOption(bitmap16::Canvas& canvas, const bitmap16::Settings& settings,
                      const char* label, int x, int y, bool selected, int minimumWidth = 64) {
  bitmap16::DialogView::drawOption(canvas, dialogTheme(settings), label, x, y, selected, minimumWidth);
}


#endif

void drawUnsavedChangesModal(bitmap16::Canvas& canvas,
                             const bitmap16::Settings& settings, int selected) {
  bitmap16::DialogView::drawSave(canvas, dialogTheme(settings), selected);
}

void drawResizeCanvasModal(bitmap16::Canvas& canvas, const bitmap16::Sketch& sketch,
                          const bitmap16::Settings& settings, uint8_t selectedSize) {
  bitmap16::DialogView::drawResize(canvas, sketch, dialogTheme(settings), selectedSize,
                                  desktopResizeAction);
}

const bitmap16::CanvasView::Assets& canvasAssets(
    const bitmap16::Settings& settings) {
  static const bitmap16::CanvasView::Assets arrowAssets = {
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
      {ICON_BUTTON_A, ICON_BUTTON_A_WIDTH, ICON_BUTTON_A_HEIGHT},
      {ICON_BUTTON_X, ICON_BUTTON_X_WIDTH, ICON_BUTTON_X_HEIGHT},
      {ICON_BUTTON_Y, ICON_BUTTON_Y_WIDTH, ICON_BUTTON_Y_HEIGHT},
      {ICON_PALETTE, ICON_PALETTE_WIDTH, ICON_PALETTE_HEIGHT},
      {ICON_BUTTON_R3, ICON_BUTTON_R3_WIDTH, ICON_BUTTON_R3_HEIGHT},
      {ICON_CHARGING_PLUG, ICON_CHARGING_PLUG_WIDTH, ICON_CHARGING_PLUG_HEIGHT},
  };
  static const bitmap16::CanvasView::Assets handAssets = {
      {ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT},
      {ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT},
      {ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT},
      {
          {ICON_BATTERY_0, 24, 24},
          {ICON_BATTERY_10, 24, 24},
          {ICON_BATTERY_50, 24, 24},
          {ICON_BATTERY_90, 24, 24},
      },
      {ICON_HAND_CURSOR, ICON_HAND_CURSOR_WIDTH, ICON_HAND_CURSOR_HEIGHT},
      {ICON_MOVE_CURSOR, ICON_MOVE_CURSOR_WIDTH, ICON_MOVE_CURSOR_HEIGHT},
      HAND_CURSOR_OFFSET_X,
      HAND_CURSOR_OFFSET_Y,
      MOVE_CURSOR_OFFSET_X,
      MOVE_CURSOR_OFFSET_Y,
      {ICON_BUTTON_A, ICON_BUTTON_A_WIDTH, ICON_BUTTON_A_HEIGHT},
      {ICON_BUTTON_X, ICON_BUTTON_X_WIDTH, ICON_BUTTON_X_HEIGHT},
      {ICON_BUTTON_Y, ICON_BUTTON_Y_WIDTH, ICON_BUTTON_Y_HEIGHT},
      {ICON_PALETTE, ICON_PALETTE_WIDTH, ICON_PALETTE_HEIGHT},
      {ICON_BUTTON_R3, ICON_BUTTON_R3_WIDTH, ICON_BUTTON_R3_HEIGHT},
      {ICON_CHARGING_PLUG, ICON_CHARGING_PLUG_WIDTH, ICON_CHARGING_PLUG_HEIGHT},
  };
  return settings.cursorStyle == bitmap16::CursorStyle::Hand
      ? handAssets
      : arrowAssets;
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
        &sketch.pixels[0][0],
        static_cast<uint8_t>(bitmap16::kMaxGridSize),
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
        static_cast<int>(index) == workspace.activeIndex(),
    });
  }
}

void present(
    bitmap16::Canvas& canvas,
    SDL_Texture* texture,
    SDL_Renderer* renderer,
    uint8_t brightness) {
  ++desktopPresentedFrames;
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  (void)texture;
  (void)renderer;
  (void)brightness;
  nativeIo.present(canvas.pixels(), canvas.width(), canvas.height());
#else
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
#endif
}

bool showBootScreen(
    bitmap16::Canvas& canvas,
    SDL_Texture* texture,
    SDL_Renderer* renderer,
    uint8_t brightness) {
  constexpr Uint32 kBootDurationMs = 1000u;
  constexpr const char* kVersion = "v" BITMAP16_APP_VERSION;
  const int imageX = (canvas.width() - BOOT_IMAGE_WIDTH) / 2;
  const int imageY = (canvas.height() - BOOT_IMAGE_HEIGHT) / 2;

  canvas.fillScreen(0x0000);
  for (int y = 0; y < BOOT_IMAGE_HEIGHT; ++y) {
    for (int x = 0; x < BOOT_IMAGE_WIDTH; ++x) {
      const uint8_t colorIndex =
          BOOT_IMAGE[y * BOOT_IMAGE_WIDTH + x];
      if (colorIndex != 2) {
        canvas.drawPixel(
            imageX + x, imageY + y, BOOT_PALETTE[colorIndex]);
      }
    }
  }

#if defined(BITMAP16_STEAM_DECK) || defined(BITMAP16_CARDPUTER_ZERO)
  const int labelY = canvas.height() - 12;
  const int labelLeft = 4;
  const int labelCenter = canvas.width() / 2;
#else
  const int labelY = imageY + BOOT_IMAGE_HEIGHT - 12;
  const int labelLeft = imageX + 4;
  const int labelCenter = imageX + BOOT_IMAGE_WIDTH / 2;
#endif
  canvas.setTextColor(0xffff);
  canvas.setTextSize(1);
  canvas.setTextAlign(bitmap16::TextAlign::Left);
  canvas.drawString(kVersion, labelLeft, labelY);
  canvas.setTextAlign(bitmap16::TextAlign::Center);
  canvas.drawString("beepbot", labelCenter, labelY);
#if defined(BITMAP16_CARDPUTER_ZERO)
  const int helpRight = canvas.width() - 4;
  canvas.setTextAlign(bitmap16::TextAlign::Right);
  canvas.drawString("HELP", helpRight, labelY);
  canvas.drawFastHLine(helpRight - 4 * 6, labelY + 8, 5, 0xffff);
#elif !defined(BITMAP16_STEAM_DECK)
  const int helpX = imageX + BOOT_IMAGE_WIDTH - 4 - (4 * 6);
  canvas.setTextAlign(bitmap16::TextAlign::Left);
  canvas.drawString("HELP", helpX, labelY);
  canvas.drawFastHLine(helpX, labelY + 9, 6, 0xffff);
#endif
  present(canvas, texture, renderer, brightness);

  const Uint32 startedAt = SDL_GetTicks();
  while (SDL_GetTicks() - startedAt <= kBootDurationMs) {
    SDL_Event event;
    while (waitEvent(&event, 0)) {
      if (event.type == SDL_QUIT) return false;
      if (event.type == SDL_KEYDOWN &&
          event.key.keysym.sym == SDLK_ESCAPE) {
        return true;
      }
    }
    SDL_Delay(10);
  }
  return true;
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
    bitmap16::Desktop::PaletteBrowser& paletteBrowser,
    bitmap16::MemoryView::State& memoryState,
    const bitmap16::MemoryView::Catalog& memoryCatalog,
    bitmap16::Editor& editor,
    bool rulersVisible,
    const bitmap16::Settings& settings,
    SDL_Texture* texture,
    SDL_Renderer* renderer,
    uint8_t resizeGridSelection = 0,
    bool presentFrame = true) {
  if (view == DesktopView::Canvas) {
    const bitmap16::Sketch& sketch = editor.sketch();
    const char* status = desktopStatus[0] == '\0' ? nullptr : desktopStatus;
#ifdef BITMAP16_CARDPUTER_ZERO
    char zoomStatus[16] = {};
    if (status == nullptr && desktopCanvasViewport.cellSize != 0) {
      const int baseCellSize = bitmap16::CanvasView::layoutFor(
          canvas.width(), canvas.height(), sketch.gridSize).cellSize;
      if (desktopCanvasViewport.cellSize > baseCellSize) {
        std::snprintf(zoomStatus, sizeof(zoomStatus), "Zoom: %dx",
                      desktopCanvasViewport.cellSize / baseCellSize);
        status = zoomStatus;
      }
    }
#endif
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
        status,
        settings.batteryDisplay ? batteryReading.percent : -1,
        desktopDrawPressed,
        desktopErasePressed,
        desktopFillPressed,
        desktopCanvasViewport.cellSize,
        desktopCanvasViewport.x,
        desktopCanvasViewport.y,
#if defined(BITMAP16_STEAM_DECK) || defined(BITMAP16_CARDPUTER_ZERO)
        true,  // Center status messages at the bottom of the screen.
#else
        false,
#endif
        false,
#ifdef BITMAP16_STEAM_DECK
        desktopControllerPrompts,
#else
        false,
#endif
        desktopPalettePressed,
#ifdef BITMAP16_STEAM_DECK
        !desktopControllerPrompts,
#else
        false,
#endif
        batteryReading.charging,
    };
    bitmap16::CanvasView::render(
        canvas, state, canvasTheme(settings), &canvasAssets(settings));
  } else if (view == DesktopView::Help) {
#ifdef BITMAP16_CARDPUTER_ZERO
    helpState.externalKeyboard = externalKeyboardActive();
#endif
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
        platformHasShakeUndo(),
        nullptr,
        nullptr,
        platformHasQuitSetting(),
        false,
        platformHasHelpSetting(), true);
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
        previewLabelUntil != 0 &&
                !SDL_TICKS_PASSED(SDL_GetTicks(), previewLabelUntil)
            ? previewLabel
            : nullptr);
  } else if (view == DesktopView::Charging) {
    const bitmap16::Sketch& sketch = editor.sketch();
    const bitmap16::ChargingView::SketchImage chargingSketch = {
        sketch.pixels,
        sketch.gridSize,
        sketch.paletteColors,
        sketch.paletteSize,
    };
    bitmap16::ChargingView::setBattery(
        chargingState, std::max(0, batteryReading.percent),
        batteryIconForPercent(std::max(0, batteryReading.percent)));
    bitmap16::ChargingView::setDecorationIcons(
        chargingState, SCREENSAVER_SAVE, SCREENSAVER_SKULL,
        batteryReading.charging ? ICON_CHARGING_PLUG : nullptr);
    bitmap16::ChargingView::render(
        canvas, chargingState, chargingTheme(), &chargingSketch);
  } else if (view == DesktopView::Palette) {
    bitmap16::PaletteView::render(
        canvas, paletteBrowser.state(), paletteBrowser.catalog(),
        paletteBrowser.activeIndex(), paletteTheme(settings),
        CARTRIDGE_GRAPHIC, paletteBrowser.status());
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
  if (resizeGridSelection != 0) {
    drawResizeCanvasModal(canvas, editor.sketch(), settings, resizeGridSelection);
  }
  if (presentFrame) {
    present(canvas, texture, renderer, settings.displayBrightness);
  }
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
  [[maybe_unused]] bool windowed = false;  // Only the Deck uses fullscreen mode.
  for (int argument = 1; argument < argc; ++argument) {
    if (std::strcmp(argv[argument], "--smoke-test") == 0) {
      smokeTest = true;
    } else if (std::strcmp(argv[argument], "--windowed") == 0) {
      windowed = true;
    } else if (!parseSize(argv[argument], width, height)) {
      SDL_Log(
          "Usage: bitmap16dx_desktop [WIDTHxHEIGHT] "
          "[--smoke-test] [--windowed]");
      return 2;
    }
  }

#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  // The shared UI still uses SDL for timing and keycodes. Its window is virtual.
  SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return 1;
  }
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  if (!nativeIo.open()) {
    SDL_Quit();
    return 1;
  }
  if (!zeroImu.open()) {
    SDL_Log("Shake undo: %s", zeroImu.error().c_str());
  }
#endif
  // Stick state is polled once per loop to provide controlled hold-repeat.
  // Per-motion events are redundant and can flood the queue on Steam Deck,
  // delaying navigation behind stale analog samples.
  SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_IGNORE);
  SDL_EventState(SDL_JOYAXISMOTION, SDL_IGNORE);

#ifdef BITMAP16_STEAM_DECK
  const int windowWidth = windowed ? width * 4 : width;
  const int windowHeight = windowed ? height * 4 : height;
  const Uint32 windowFlags = windowed
      ? SDL_WINDOW_SHOWN
      : SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS |
          SDL_WINDOW_FULLSCREEN_DESKTOP;
#elif defined(BITMAP16_CARDPUTER_ZERO_DEVICE)
  const int windowWidth = width;
  const int windowHeight = height;
  const Uint32 windowFlags =
      SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS |
      SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
  const int windowWidth = width * 3;
  const int windowHeight = height * 3;
  const Uint32 windowFlags = SDL_WINDOW_SHOWN;
#endif

  // The artwork cursor supplies pointer feedback in every SDL build.
  SDL_ShowCursor(SDL_DISABLE);
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
  // Keep canvas shortcuts out of text-entry mode. The import dialog enables
  // system text input only while its field has focus.
  SDL_StopTextInput();
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  const Uint32 rendererFlags = SDL_RENDERER_SOFTWARE;
#else
  const Uint32 rendererFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#endif
  SDL_Renderer* renderer = window == nullptr
      ? nullptr
      : SDL_CreateRenderer(
            window,
            -1,
            rendererFlags);
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
  const auto rumble = [&](Uint16 low, Uint16 high, Uint32 duration) {
#ifdef BITMAP16_STEAM_DECK
    if (controller != nullptr) {
      SDL_GameControllerRumble(controller, low, high, duration);
    }
#else
    (void)low;
    (void)high;
    (void)duration;
#endif
  };

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
  if (!platformHasLedMatrixControls()) settings.matrixEnabled = false;
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  nativeIo.setDisplayWarmth(settings.displayWarmth);
#endif
#ifdef BITMAP16_STEAM_DECK
  // SteamOS owns the physical display brightness. Applying the embedded
  // device's saved software brightness here dims the rendered colors a
  // second time and makes both Deck output and windowed previews inaccurate.
  settings.displayBrightness = 100;
#endif
  if (!showBootScreen(
          canvas,
          texture,
          renderer,
          settings.displayBrightness)) {
    if (controller != nullptr) SDL_GameControllerClose(controller);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
  }
  const bitmap16::Sketch& initialSketch = editor.sketch();
  const bitmap16::PreviewView::Image previewImage = {
      initialSketch.pixels,
      initialSketch.gridSize,
      initialSketch.paletteColors,
      initialSketch.paletteSize,
  };
  batteryReading = readPlatformBattery();
  const int initialBatteryPercent = std::max(0, batteryReading.percent);
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
  bitmap16::Desktop::PaletteBrowser paletteBrowser(workspace, editor);
  auto& paletteState = paletteBrowser.state();
#ifdef BITMAP16_LOSPEC
  std::string lastPaletteLookup;
#endif
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
#if !defined(BITMAP16_CARDPUTER_ZERO_DEVICE) && !defined(BITMAP16_STEAM_DECK)
  bitmap16::Desktop::MatrixSimulator desktopMatrix;
  if (platformHasLedMatrixControls()) {
    if (desktopMatrix.create()) {
      desktopMatrix.setEnabled(settings.matrixEnabled);
      matrixSimulator = &desktopMatrix;
    } else {
      SDL_Log("RGB matrix simulator creation failed: %s", SDL_GetError());
    }
  }
#endif
  bool galleryMode = false;
  bool galleryAutoAdvance = false;
  int galleryIndex = 0;
  Uint32 galleryLastAdvance = SDL_GetTicks();
  Uint32 paletteCompletionAt = 0;
  Uint32 paletteFrameAt = SDL_GetTicks();
  constexpr Uint32 paletteFrameInterval = 16u;
  Uint32 palettePresentedAt = 0;
  bool paletteHasPresented = false;
  bool paletteRenderPending = false;
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
      paletteBrowser,
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
            platformHasShakeUndo(), false, false, false, true) - 1;
    bitmap16::SettingsView::activate(
        settingsState,
        settings,
        false,
        platformHasLedMatrixControls(),
        platformHasShakeUndo(), false, false, false, true);
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteBrowser,
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
        paletteBrowser,
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
        paletteBrowser,
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
        paletteBrowser,
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
        paletteBrowser,
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
          paletteBrowser,
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

  bitmap16::Desktop::AudioPlayer audioPlayer;
  char* executableDirectory = SDL_GetBasePath();
  const std::filesystem::path musicDirectory = executableDirectory == nullptr
      ? std::filesystem::path{} : std::filesystem::path(executableDirectory) / "music";
  SDL_free(executableDirectory);
  audioPlayer.open(musicDirectory);
  audioPlayer.setMusicVolume(settings.musicVolume);
  audioPlayer.setSoundEffectsVolume(settings.soundEffectsVolume);
  audioPlayer.setSoundEffectsEnabled(settings.soundEffectsEnabled);
  audioPlayer.setMusicEnabled(settings.musicEnabled);
  appAudio = &audioPlayer;
  musicEventType = SDL_RegisterEvents(1);
  musicShortcut = [&](SDL_Keycode key) {
    bool success = true;
    if (key == SDLK_AUDIOPLAY) success = audioPlayer.setMusicEnabled(!audioPlayer.musicEnabled());
    else if (key == SDLK_AUDIOPREV) success = audioPlayer.previousTrack();
    else success = audioPlayer.nextTrack();
    settings.musicEnabled = audioPlayer.musicEnabled();
    workspace.saveSettings();
    const std::string message = !success ? "Music unavailable" :
        key == SDLK_AUDIOPLAY ? (settings.musicEnabled ? "Music playing" : "Music paused") :
        "Music: " + audioPlayer.trackName();
    setDesktopStatus(message.c_str());
  };
  bool mouseDrawHeld = false;
  bool mouseEraseHeld = false;
  bool pointerInsideCanvas = false;
  using PaletteExitAction = bitmap16::Desktop::PaletteBrowser::Action;
  uint8_t pendingPaletteSize = 0;
  const auto requestPaletteExit = [&](PaletteExitAction action) {
    paletteBrowser.request(action, canvas, SDL_GetTicks(), pendingPaletteSize);
  };
  bool paletteWasVisible = false;
  auto menuSoundView = currentView;
  bool cartridgeApplied = false;
  bool sketchOpened = false;
  const auto renderNow = [&](uint8_t resizeGridSelection = 0,
                             bool presentFrame = true) {
    if (menuSoundView != currentView) {
      const auto cue = bitmap16::Desktop::menuTransitionCue(
          menuSoundView, currentView, returnView, cartridgeApplied, sketchOpened);
      if (cue == bitmap16::Desktop::MenuCue::Open) audioPlayer.playMenuOpen();
      if (cue == bitmap16::Desktop::MenuCue::Close) audioPlayer.playMenuClose();
      if (cue == bitmap16::Desktop::MenuCue::SketchOpen) audioPlayer.playSketchOpen();
      sketchOpened = false;
      menuSoundView = currentView;
      if (currentView == DesktopView::Palette && !paletteState.insertionAnimating)
        cartridgeApplied = false;
    }
    if (currentView == DesktopView::Palette && !paletteWasVisible) {
      bitmap16::PaletteView::beginEntrance(paletteState, width);
      paletteFrameAt = SDL_GetTicks();
    }
    paletteWasVisible = currentView == DesktopView::Palette;
    if (currentView == DesktopView::Palette && presentFrame) {
      const Uint32 now = SDL_GetTicks();
      if (paletteHasPresented && now - palettePresentedAt < paletteFrameInterval) {
        // Keep the latest state; input events must not bypass frame pacing.
        paletteRenderPending = true;
        return;
      }
      palettePresentedAt = now;
      paletteHasPresented = true;
      paletteRenderPending = false;
    } else if (currentView != DesktopView::Palette) {
      paletteHasPresented = false;
      paletteRenderPending = false;
    }
    if (currentView != DesktopView::Canvas) {
      mouseDrawHeld = mouseEraseHeld = pointerInsideCanvas = false;
    }
    if (currentView != DesktopView::Canvas) editor.endUndoGroup();
    renderCurrentView(
        currentView,
        canvas,
        helpState,
        settingsState,
        previewState,
        previewImage,
        chargingState,
        chargingSketch,
        paletteBrowser,
        memoryState,
        memoryCatalog,
        editor,
        rulersVisible,
        settings,
        texture,
        renderer,
        resizeGridSelection,
        presentFrame);
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
    const Uint32 now = SDL_GetTicks();
    if (currentView != DesktopView::Palette) { paletteFrameAt = now; return; }
    const Uint32 elapsed = now - paletteFrameAt;
    if (elapsed < paletteFrameInterval) {
      if (paletteRenderPending) renderNow();
      return;
    }
    paletteFrameAt = now;
    const bitmap16::PaletteView::AnimationResult result = paletteBrowser.advance(
        static_cast<float>(elapsed > 250u ? paletteFrameInterval : std::min(elapsed, 100u)), width);
    if (result == bitmap16::PaletteView::AnimationResult::ExitComplete) {
      currentView = DesktopView::Canvas;
      paletteHasPresented = false;
      renderNow();
    } else if (result ==
        bitmap16::PaletteView::AnimationResult::SelectionComplete) {
      if (paletteCompletionAt == 0) {
        paletteCompletionAt = SDL_GetTicks();
        audioPlayer.playCartridgeInsert();
        renderNow();
      } else if (SDL_GetTicks() - paletteCompletionAt >= 200u) {
        paletteState.insertionAnimating = false;
        paletteCompletionAt = 0;
        currentView = DesktopView::Canvas;
        renderNow();
      }
    } else if (result != bitmap16::PaletteView::AnimationResult::Idle) {
      renderNow();
    } else if (paletteRenderPending) {
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
#ifndef BITMAP16_CARDPUTER_ZERO_DEVICE
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
#endif

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
  bool controllerUndoReleaseConsumed = false;
  bool controllerSaveChordLatched = false;
  bool focusNewestSketchOnNextMemoryOpen = false;
  bool previewLeftTriggerLatched = false;
  bool previewRightTriggerLatched = false;
  bool quitRequested = false;
  bitmap16::Desktop::SketchStorage sketchStorage(workspace, editor);
  bitmap16::SketchWorkflow sketchWorkflow(editor, sketchStorage);
  using SketchAction = bitmap16::SketchWorkflow::Action;
  using SketchChoice = bitmap16::SketchWorkflow::Choice;
  using SketchResult = bitmap16::SketchWorkflow::Result;
  const auto confirmSketchDeletion = [&]() {
    bool deleteSelected = false;
    const auto draw = [&]() {
      renderNow(0, false);
      bitmap16::DialogView::drawDelete(canvas, dialogTheme(settings), deleteSelected);
      present(canvas, texture, renderer, settings.displayBrightness);
    };
    draw();
    SDL_Event confirmation;
    while (waitEvent(&confirmation)) {
      SDL_Keycode key = SDLK_UNKNOWN;
      if (confirmation.type == SDL_KEYDOWN && confirmation.key.repeat == 0) {
        key = confirmation.key.keysym.sym;
#ifdef BITMAP16_CARDPUTER_ZERO
        key = bitmap16::Zero::navigationKey(confirmation);
#endif
      } else if (confirmation.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (confirmation.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_A: key = SDLK_RETURN; break;
          case SDL_CONTROLLER_BUTTON_B: key = SDLK_ESCAPE; break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = SDLK_LEFT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = SDLK_RIGHT; break;
          default: break;
        }
      } else if (confirmation.type == SDL_QUIT ||
                 (confirmation.type == SDL_WINDOWEVENT &&
                  confirmation.window.event == SDL_WINDOWEVENT_CLOSE)) {
        SDL_PushEvent(&confirmation);
        renderNow();
        return false;
      }
      if (key == SDLK_ESCAPE || key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        renderNow();
        return key != SDLK_ESCAPE && deleteSelected;
      }
      if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        const bool nextSelection = key == SDLK_RIGHT;
        if (nextSelection != deleteSelected) audioPlayer.playNavigationTick();
        deleteSelected = nextSelection;
        draw();
      }
    }
    renderNow();
    return false;
  };
  const auto requestSketchAction = [&](SketchAction action, const std::string& key = std::string{}) {
    SketchResult result = sketchWorkflow.request(action, settings.saveWarnings, key);
    const bool showedDialog = result == SketchResult::AwaitingChoice;
    const auto draw = [&]() {
      renderNow(0, false);
      drawUnsavedChangesModal(canvas, settings, static_cast<int>(sketchWorkflow.choice()));
      present(canvas, texture, renderer, settings.displayBrightness);
    };
    if (showedDialog) draw();
    while (sketchWorkflow.awaitingChoice()) {
      SDL_Event modalEvent;
      if (waitEvent(&modalEvent) == 0) continue;
      SDL_Keycode modalKey = SDLK_UNKNOWN;
      if (modalEvent.type == SDL_KEYDOWN && modalEvent.key.repeat == 0) {
        modalKey = modalEvent.key.keysym.sym;
#ifdef BITMAP16_CARDPUTER_ZERO
        modalKey = bitmap16::Zero::navigationKey(modalEvent);
#endif
      } else if (modalEvent.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (modalEvent.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_A: modalKey = SDLK_RETURN; break;
          case SDL_CONTROLLER_BUTTON_B: modalKey = SDLK_ESCAPE; break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT: modalKey = SDLK_LEFT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: modalKey = SDLK_RIGHT; break;
          default: break;
        }
      } else if (modalEvent.type == SDL_QUIT ||
                 (modalEvent.type == SDL_WINDOWEVENT &&
                  modalEvent.window.event == SDL_WINDOWEVENT_CLOSE)) {
        SDL_PushEvent(&modalEvent);
        result = sketchWorkflow.choose(SketchChoice::Cancel);
        break;
      }
      if (modalKey == SDLK_LEFT || modalKey == SDLK_RIGHT) {
        const auto previousChoice = sketchWorkflow.choice();
        sketchWorkflow.moveChoice(modalKey == SDLK_LEFT ? -1 : 1);
        if (sketchWorkflow.choice() != previousChoice) audioPlayer.playNavigationTick();
        draw();
      } else if (modalKey == SDLK_RETURN || modalKey == SDLK_KP_ENTER) {
        const SketchChoice choice = sketchWorkflow.choice();
        result = sketchWorkflow.choose(choice);
        if (choice == SketchChoice::Save && result != SketchResult::SaveFailed) {
          audioPlayer.playSketchSave();
          refreshMemoryCatalog();
          setDesktopStatus("Saved");
        }
        if (result == SketchResult::SaveFailed) {
          // SDL closes a failed-save dialog; ADV keeps it open for retry.
          sketchWorkflow.choose(SketchChoice::Cancel);
          break;
        }
      } else if (modalKey == SDLK_ESCAPE) {
        result = sketchWorkflow.choose(SketchChoice::Cancel);
      }
    }
    if (result == SketchResult::Completed) {
      if (action != SketchAction::Quit) desktopCanvasViewport = {};
      refreshMemoryCatalog();
    } else if (result == SketchResult::SaveFailed) {
      setDesktopStatus("Failed to save");
    } else if (result == SketchResult::OpenFailed) {
      setDesktopStatus("Failed to load");
    }
    if (showedDialog || result == SketchResult::OpenFailed) renderNow();
    return result == SketchResult::Completed;
  };
  const auto chooseGridSize = [&]() {
    constexpr uint8_t sizes[] = {8, 16, 32};
    const uint8_t currentSize = editor.sketch().gridSize;
    int selection = currentSize == 8 ? 0 : currentSize == 16 ? 1 : 2;
    const auto originalViewport = desktopCanvasViewport;
    desktopCanvasViewport = {};
    desktopResizeAction = -1;
    renderNow(sizes[selection]);

    uint8_t chosenSize = currentSize;
    SDL_Event modalEvent;
    while (waitEvent(&modalEvent)) {
      if (modalEvent.type == SDL_QUIT ||
          (modalEvent.type == SDL_WINDOWEVENT &&
           modalEvent.window.event == SDL_WINDOWEVENT_CLOSE)) {
        SDL_PushEvent(&modalEvent);
        break;
      }
      SDL_Keycode key = SDLK_UNKNOWN;
      bool repeated = false;
      if (modalEvent.type == SDL_KEYDOWN) {
        key = modalEvent.key.keysym.sym;
#ifdef BITMAP16_CARDPUTER_ZERO
        key = bitmap16::Zero::navigationKey(modalEvent);
#else
        if (key == SDLK_x) key = SDLK_UP;
        if (key == SDLK_f) key = SDLK_DOWN;
        if (key == SDLK_z) key = SDLK_LEFT;
        if (key == SDLK_c) key = SDLK_RIGHT;
#endif
        repeated = modalEvent.key.repeat != 0;
      } else if (modalEvent.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (modalEvent.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_A: key = SDLK_RETURN; break;
          case SDL_CONTROLLER_BUTTON_B: key = SDLK_ESCAPE; break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = SDLK_LEFT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = SDLK_RIGHT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_UP: key = SDLK_UP; break;
          case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = SDLK_DOWN; break;
          default: break;
        }
      } else continue;
      if (key == SDLK_ESCAPE && !repeated) break;
      const int previousSelection = selection;
      const int previousAction = desktopResizeAction;
      if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !repeated) {
        if (desktopResizeAction < 0) desktopResizeAction = 1;
        else {
          if (desktopResizeAction == 1) chosenSize = sizes[selection];
          break;
        }
      } else if (key == SDLK_UP) {
        desktopResizeAction = -1;
      } else if (key == SDLK_DOWN) {
        if (desktopResizeAction < 0) desktopResizeAction = 1;
      } else if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        const int delta = key == SDLK_LEFT ? -1 : 1;
        if (desktopResizeAction < 0) selection = std::clamp(selection + delta, 0, 2);
        else desktopResizeAction = std::clamp(desktopResizeAction + delta, 0, 1);
      } else {
        continue;
      }
      if (selection != previousSelection || desktopResizeAction != previousAction)
        audioPlayer.playNavigationTick();
      renderNow(sizes[selection]);
    }

    desktopCanvasViewport = originalViewport;
    desktopResizeAction = -1;
    return editor.resizeGrid(chosenSize);
  };
  if (smokeTest) {
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    const auto musicView = currentView;
    const bool musicWasEnabled = settings.musicEnabled;
    if (audioPlayer.trackCount() > 0 && SDL_WasInit(SDL_INIT_AUDIO)) {
      // The same wait path services every screen and modal, including import.
      pushKeyEvent(SDLK_AUDIOPLAY, false);
      SDL_Event mediaEvent{};
      if (!waitEvent(&mediaEvent, 1000) || mediaEvent.type != musicEventType ||
          settings.musicEnabled == musicWasEnabled || currentView != musicView) {
        SDL_Log("Music shortcut smoke failed: event=%u expected=%u enabled=%d before=%d tracks=%zu",
                mediaEvent.type, musicEventType, settings.musicEnabled, musicWasEnabled, audioPlayer.trackCount());
        return 1;
      }
#ifdef BITMAP16_STEAM_DECK
      const auto pushMusicButton = [](Uint32 type) {
        SDL_Event button{};
        button.type = type;
        button.cbutton.button = SDL_CONTROLLER_BUTTON_LEFTSTICK;
        button.cbutton.state = type == SDL_CONTROLLERBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
        SDL_PushEvent(&button);
      };
      // Verify a press toggles once and the release does not reach other controls.
      SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
      pushMusicButton(SDL_CONTROLLERBUTTONDOWN);
      if (!waitEvent(&mediaEvent, 1000) || mediaEvent.type != musicEventType ||
          settings.musicEnabled != musicWasEnabled || currentView != musicView) {
        SDL_Log("L3 music toggle smoke failed");
        return 1;
      }
      pushMusicButton(SDL_CONTROLLERBUTTONUP);
      if (waitEvent(&mediaEvent, 0) || settings.musicEnabled != musicWasEnabled) {
        SDL_Log("L3 release must not toggle music or escape into view controls");
        return 1;
      }
      // Toggle back from inside a real modal without affecting its result.
      pushMusicButton(SDL_CONTROLLERBUTTONDOWN);
      pushMusicButton(SDL_CONTROLLERBUTTONUP);
#endif
      pushKeyEvent(SDLK_AUDIOPREV, false);
      pushKeyEvent(SDLK_AUDIONEXT, false);
      pushKeyEvent(SDLK_ESCAPE, false);
      if (confirmSketchDeletion() || currentView != musicView ||
          settings.musicEnabled == musicWasEnabled) {
        SDL_Log("Music controls changed the deletion dialog outcome");
        return 1;
      }
      pushKeyEvent(SDLK_AUDIOPLAY, false);
      if (!waitEvent(&mediaEvent, 1000) || settings.musicEnabled != musicWasEnabled) {
        SDL_Log("Music pause smoke failed: event=%u enabled=%d before=%d",
                mediaEvent.type, settings.musicEnabled, musicWasEnabled);
        return 1;
      }
      SDL_Log("Music shortcut smoke checks passed");
    }
    const bool warnings = settings.saveWarnings;
    settings.saveWarnings = true;
    if (!editor.draw()) return 1;
    pushKeyEvent(SDLK_ESCAPE, false);
    const uint64_t beforeSaveWarning = desktopPresentedFrames;
    if (requestSketchAction(SketchAction::NewSketch) || !editor.canUndo() ||
        !sketchWorkflow.hasUnsavedChanges()) return 1;
    if (desktopPresentedFrames - beforeSaveWarning != 2) {
      SDL_Log("Save warning must present once on open and once on close; got %llu frames",
              static_cast<unsigned long long>(desktopPresentedFrames - beforeSaveWarning));
      return 1;
    }
    const uint64_t beforeSaveFocusChange = desktopPresentedFrames;
    pushKeyEvent(SDLK_LEFT, false);
    pushKeyEvent(SDLK_RETURN, false);
    if (!requestSketchAction(SketchAction::NewSketch) || editor.canUndo() ||
        sketchWorkflow.hasUnsavedChanges()) return 1;
    if (desktopPresentedFrames - beforeSaveFocusChange != 3) {
      SDL_Log("Save warning must present once per focus change");
      return 1;
    }
    if (!editor.draw()) return 1;
    pushKeyEvent(SDLK_ESCAPE, false);
    if (requestSketchAction(SketchAction::Quit) || !editor.canUndo()) return 1;
    settings.saveWarnings = false;
    if (!requestSketchAction(SketchAction::NewSketch) || editor.canUndo()) return 1;
    settings.saveWarnings = warnings;
  }
#ifdef BITMAP16_LOSPEC
  const auto enterPaletteLookup = [&](std::string& input) {
    int actionFocus = -1;
#ifdef BITMAP16_STEAM_DECK
    struct TextInputSession {
      bool focused = true;
      TextInputSession() { SDL_StartTextInput(); }
      ~TextInputSession() { SDL_StopTextInput(); }
      void focus(bool next) {
        if (focused == next) return;
        focused = next;
        if (focused) SDL_StartTextInput();
        else SDL_StopTextInput();
      }
    } textInputSession;
#endif
    constexpr const char* examples[] = {
        "endesga-16", "vanilla-milkshake", "dreamscape8", "cl8uds", "citrink",
        "mist-gb", "bubblegum-16", "commodore64", "slso8", "island-joy-16"};
    constexpr std::size_t exampleCount = sizeof(examples) / sizeof(examples[0]);
    static std::random_device randomSource;
    static std::size_t previousExampleIndex = exampleCount;
    std::uniform_int_distribution<std::size_t> choice(
        0, exampleCount - (previousExampleIndex == exampleCount ? 1 : 2));
    std::size_t exampleIndex = choice(randomSource);
    if (exampleIndex >= previousExampleIndex) ++exampleIndex;
    previousExampleIndex = exampleIndex;
    std::string hint;
    constexpr Uint32 cursorBlinkMs = 500;
    Uint32 cursorEpoch = SDL_GetTicks();
    const auto fieldFocused = [&]() { return actionFocus < 0; };
    const auto draw = [&]() {
#ifdef BITMAP16_STEAM_DECK
      textInputSession.focus(fieldFocused());
#endif
      renderNow(0, false);
      const auto theme = helpTheme(settings);
      const int dialogHeight = hint.empty() ? 124 : 144;
      const auto frame = drawDialogFrame(canvas, settings, "IMPORT FROM LOSPEC", dialogHeight);
      const auto iconTheme = canvasTheme(settings);
      for (int pixel = 0; pixel < 24 * 24; ++pixel) {
        const int value = (ICON_LOSPEC[pixel / 4] >> ((3 - pixel % 4) * 2)) & 3;
        if (value == 1 || value == 2)
          canvas.drawPixel(canvas.width() / 2 - 12 + pixel % 24,
                           frame.y + 12 + pixel / 24,
                           value == 1 ? iconTheme.iconDark : iconTheme.iconLight);
      }
      canvas.setTextColor(theme.textSecondary);
      canvas.setTextAlign(bitmap16::TextAlign::Left);
      const int fieldX = frame.x + 12;
      const int fieldWidth = frame.width - 24;
      canvas.drawRect(fieldX, frame.y + 60, fieldWidth, 20, theme.text);
      if (fieldFocused())
        canvas.drawRect(fieldX + 1, frame.y + 61, fieldWidth - 2, 18, theme.text);
      const auto visibleCharacters = static_cast<std::size_t>((fieldWidth - 10) / 6);
      const auto visible = input.substr(
          input.size() > visibleCharacters ? input.size() - visibleCharacters : 0);
      canvas.setTextColor(input.empty() ? theme.textSecondary : theme.text);
      canvas.drawString(input.empty() ? examples[exampleIndex] : visible.c_str(),
                        fieldX + 4, frame.y + 66);
      if (fieldFocused() && ((SDL_GetTicks() - cursorEpoch) / cursorBlinkMs) % 2 == 0) {
        const int cursorX = input.empty() ? fieldX + 3 :
            fieldX + 5 + static_cast<int>(visible.size()) * 6;
        canvas.fillRect(cursorX, frame.y + 65, 1, 9, theme.text);
      }
      canvas.setTextColor(theme.text);
      canvas.setTextAlign(bitmap16::TextAlign::Center);
      if (!hint.empty()) canvas.drawString(hint.c_str(), canvas.width() / 2, frame.y + 86);
      drawDialogOption(canvas, settings, "CANCEL", bitmap16::DialogView::optionX(canvas, 0, 2),
                       frame.y + frame.height - 24, actionFocus == 0);
      drawDialogOption(canvas, settings, "IMPORT", bitmap16::DialogView::optionX(canvas, 1, 2),
                       frame.y + frame.height - 24, actionFocus == 1);
      present(canvas, texture, renderer, settings.displayBrightness);
    };
    const auto paste = [&]() {
      cursorEpoch = SDL_GetTicks();
      actionFocus = -1;
      char* text = SDL_GetClipboardText();
      if (text) {
        input.append(text, std::min<std::size_t>(std::strlen(text), 2048 - input.size()));
        SDL_free(text);
      }
    };
    const auto finish = [&](bool submit) {
      if (submit) {
        std::string slug;
        if (!bitmap16::Lospec::lookupSlug(input, slug)) {
          hint = "Enter a Lospec name or URL";
          actionFocus = -1;
          return false;
        }
      }
      paletteHasPresented = false;
      return true;
    };
    draw();
    SDL_Event entryEvent;
    while (true) {
      const int timeout = fieldFocused() ? static_cast<int>(
          cursorBlinkMs - (SDL_GetTicks() - cursorEpoch) % cursorBlinkMs) : -1;
      if (!waitEvent(&entryEvent, timeout)) { draw(); continue; }
      if (entryEvent.type == SDL_QUIT || (entryEvent.type == SDL_WINDOWEVENT &&
          entryEvent.window.event == SDL_WINDOWEVENT_CLOSE)) {
        SDL_PushEvent(&entryEvent);
        finish(false);
        return false;
      }
#ifdef BITMAP16_STEAM_DECK
      if (entryEvent.type == SDL_TEXTINPUT) {
        if (fieldFocused()) {
          for (const unsigned char character : entryEvent.text.text) {
            if (character == 0) break;
            if (character >= 32 && character <= 126 && input.size() < 2048)
              input += static_cast<char>(character);
          }
          cursorEpoch = SDL_GetTicks();
          draw();
        }
        continue;
      }
#endif
      SDL_Keycode key = SDLK_UNKNOWN;
#ifndef BITMAP16_STEAM_DECK
      bool shift = false;
#endif
      if (entryEvent.type == SDL_KEYDOWN) {
        key = entryEvent.key.keysym.sym;
#ifndef BITMAP16_STEAM_DECK
        shift = (entryEvent.key.keysym.mod & KMOD_SHIFT) != 0;
#endif
        if ((entryEvent.key.keysym.mod & KMOD_CTRL) != 0) {
          if (key == SDLK_v) { paste(); draw(); }
          continue;
        }
      } else if (entryEvent.type == SDL_CONTROLLERBUTTONDOWN) {
        switch (entryEvent.cbutton.button) {
          case SDL_CONTROLLER_BUTTON_A: key = SDLK_RETURN; break;
          case SDL_CONTROLLER_BUTTON_B: key = SDLK_ESCAPE; break;
          case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = SDLK_LEFT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = SDLK_RIGHT; break;
          case SDL_CONTROLLER_BUTTON_DPAD_UP: key = SDLK_UP; break;
          case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = SDLK_DOWN; break;
          default: break;
        }
      } else continue;
      cursorEpoch = SDL_GetTicks();
      const int previousFocus = actionFocus;
      if (key == SDLK_ESCAPE) { finish(false); return false; }
      if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
          (entryEvent.type != SDL_KEYDOWN || entryEvent.key.repeat == 0)) {
        if (actionFocus < 0) actionFocus = 1;
        else if (actionFocus == 0) { finish(false); return false; }
        else if (finish(true)) return true;
      } else if (key == SDLK_BACKSPACE || key == SDLK_DELETE) {
        if (!input.empty()) input.pop_back();
        actionFocus = -1;
      } else if (key == SDLK_UP) {
        actionFocus = -1;
      } else if (key == SDLK_DOWN) {
        if (actionFocus < 0) actionFocus = 1;
      } else if (key == SDLK_LEFT || key == SDLK_RIGHT) {
        if (actionFocus >= 0) actionFocus = key == SDLK_LEFT ? 0 : 1;
#ifndef BITMAP16_STEAM_DECK
      } else if (key >= 32 && key <= 126 && input.size() < 2048) {
        char character = static_cast<char>(key);
        if (shift) {
          if (character >= 'a' && character <= 'z') character -= 'a' - 'A';
          else {
            const char* plain = "1234567890-=[];'/,`.\\";
            const char* shifted = "!@#$%^&*()_+{}:\"?<~>|";
            const char* found = std::strchr(plain, character);
            if (found) character = shifted[found - plain];
          }
        }
        input += character;
        actionFocus = -1;
#endif
      }
      if (actionFocus != previousFocus && (key == SDLK_UP || key == SDLK_DOWN ||
          key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_RETURN || key == SDLK_KP_ENTER))
        audioPlayer.playNavigationTick();
      draw();
    }
  };
  if (smokeTest) {
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    pushKeyEvent(SDLK_RETURN, false);
    if (confirmSketchDeletion()) return 1;
    pushKeyEvent(SDLK_RIGHT, false);
    pushKeyEvent(SDLK_ESCAPE, false);
    if (confirmSketchDeletion()) return 1;
    pushKeyEvent(SDLK_RIGHT, false);
    pushKeyEvent(SDLK_RETURN, false);
    if (!confirmSketchDeletion()) return 1;
    const auto originalGridSize = editor.sketch().gridSize;
    pushKeyEvent(SDLK_RIGHT, false);
    pushKeyEvent(SDLK_DOWN, false);
    pushKeyEvent(SDLK_LEFT, false);
    pushKeyEvent(SDLK_RETURN, false);
    chooseGridSize();
    if (editor.sketch().gridSize != originalGridSize) return 1;
    pushKeyEvent(originalGridSize == 32 ? SDLK_LEFT : SDLK_RIGHT, false);
    pushKeyEvent(SDLK_RETURN, false);
    pushKeyEvent(SDLK_RETURN, false);
    chooseGridSize();
    if (editor.sketch().gridSize == originalGridSize) return 1;
    editor.resizeGrid(originalGridSize);
    std::string input;
    const auto beforeImportView = currentView;
    currentView = DesktopView::Palette;
    paletteHasPresented = false;
    renderNow();
    const uint64_t beforeImport = desktopPresentedFrames;
    pushKeyEvent(SDLK_ESCAPE, false);
    if (enterPaletteLookup(input)) return 1;
    if (desktopPresentedFrames - beforeImport != 1) {
      SDL_Log("Import redraw presented %llu frames; expected one composed dialog",
              static_cast<unsigned long long>(desktopPresentedFrames - beforeImport));
      return 1;
    }
    const uint64_t beforeFocusChanges = desktopPresentedFrames;
    pushKeyEvent(SDLK_DOWN, false);
    pushKeyEvent(SDLK_UP, false);
    pushKeyEvent(SDLK_ESCAPE, false);
    if (enterPaletteLookup(input) || desktopPresentedFrames - beforeFocusChanges != 3) {
      SDL_Log("Import focus changes must present one composed frame per redraw");
      return 1;
    }
    currentView = beforeImportView;
    pushKeyEvent(SDLK_RETURN, false);
    pushKeyEvent(SDLK_ESCAPE, false);
    if (enterPaletteLookup(input) || !input.empty()) {
      SDL_Log("Palette placeholder must not be submitted as input");
      return 1;
    }
#ifdef BITMAP16_STEAM_DECK
    SDL_version textTestVersion{};
    SDL_GetVersion(&textTestVersion);
    // SDL2-compat reserves patch versions >= 50 and cannot inject SDL_TEXTINPUT.
    // Real system text input works; this synthetic-event check needs native SDL2.
    if (textTestVersion.patch >= 50) {
      SDL_Log("System text-input smoke check requires native SDL2; SDL2-compat cannot inject text events");
      return 1;
    }
    const auto pushText = [&](const char* text) {
      SDL_StartTextInput();
      SDL_Event event{};
      event.type = SDL_TEXTINPUT;
      event.text.windowID = SDL_GetWindowID(window);
      std::snprintf(event.text.text, sizeof(event.text.text), "%s", text);
      SDL_PushEvent(&event);
    };
#endif
    for (const char character : std::string("sweetie-16")) pushKeyEvent(character, false);
#ifdef BITMAP16_STEAM_DECK
    pushText("sweetie-16");
#endif
    pushKeyEvent(SDLK_RETURN, false);
    pushKeyEvent(SDLK_RETURN, false);
    if (!enterPaletteLookup(input) || input != "sweetie-16") {
      SDL_Log("Palette lookup keyboard smoke test failed");
      return 1;
    }
    const auto button = [](Uint8 value) {
      SDL_Event event{};
      event.type = SDL_CONTROLLERBUTTONDOWN;
      event.cbutton.button = value;
      event.cbutton.state = SDL_PRESSED;
      SDL_PushEvent(&event);
    };
#ifdef BITMAP16_STEAM_DECK
    if (SDL_IsTextInputActive()) return 1;
    // Resize and filter controls must work without keyboard input.
    const auto beforeControllerResize = editor.sketch().gridSize;
    const int resizeDirection = beforeControllerResize == 32 ? SDL_CONTROLLER_BUTTON_DPAD_LEFT :
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    button(static_cast<Uint8>(resizeDirection));
    button(SDL_CONTROLLER_BUTTON_A);
    button(SDL_CONTROLLER_BUTTON_DPAD_UP);
    button(SDL_CONTROLLER_BUTTON_B);
    if (chooseGridSize() || editor.sketch().gridSize != beforeControllerResize) return 1;
    button(static_cast<Uint8>(resizeDirection));
    button(SDL_CONTROLLER_BUTTON_A);
    button(SDL_CONTROLLER_BUTTON_A);
    if (!chooseGridSize() || editor.sketch().gridSize == beforeControllerResize) return 1;
    editor.resizeGrid(beforeControllerResize);
    if (paletteFilterKey(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0, true) != SDLK_4 ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 4, true) != SDLK_8 ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 8, false) != SDLK_1 ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 16, false) != SDLK_0 ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0, true) != SDLK_1 ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_DPAD_UP, 8, true) != SDLK_u ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_DPAD_UP, 8, false) != SDLK_UNKNOWN ||
        paletteFilterKey(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 8, true) != SDLK_0) return 1;
    input.clear();
    pushText("a");
    button(SDL_CONTROLLER_BUTTON_A);
    button(SDL_CONTROLLER_BUTTON_A);
    if (!enterPaletteLookup(input) || input != "a" || SDL_IsTextInputActive()) {
      SDL_Log("Palette system text input/controller smoke test failed");
      return 1;
    }
#endif
    button(SDL_CONTROLLER_BUTTON_B);
    if (enterPaletteLookup(input)) {
      SDL_Log("Palette lookup cancel smoke test failed");
      return 1;
    }
#ifdef BITMAP16_STEAM_DECK
    if (SDL_IsTextInputActive()) return 1;
#endif
  }
#endif
  bool running = !smokeTest;
#ifdef BITMAP16_STEAM_DECK
  unsigned previousTriggerMask = 0;
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
  constexpr Uint32 kBatteryRefreshMs = 2000;
  Uint32 nextBatteryRefresh = SDL_GetTicks() + kBatteryRefreshMs;
#endif
  while (running) {
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    if (zeroHomeRequested) break;
#endif
    SDL_PumpEvents();
#ifdef BITMAP16_LOSPEC
    const auto paletteUpdate = paletteBrowser.poll(currentView == DesktopView::Palette);
    if (paletteUpdate == bitmap16::Desktop::PaletteBrowser::Update::PreviewReady) {
      currentView = DesktopView::Palette;
      // The preview is a new list even when lookup began in the palette view.
      paletteWasVisible = false;
      paletteHasPresented = false;
      renderNow();
    } else if (paletteUpdate == bitmap16::Desktop::PaletteBrowser::Update::Changed &&
               currentView == DesktopView::Palette) {
      renderNow();
    }
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    if ((currentView == DesktopView::Canvas && settings.batteryDisplay) ||
        currentView == DesktopView::Charging) {
      const Uint32 now = SDL_GetTicks();
      if (SDL_TICKS_PASSED(now, nextBatteryRefresh)) {
        nextBatteryRefresh = now + kBatteryRefreshMs;
        const BatteryReading previous = batteryReading;
        batteryReading = readPlatformBattery();
        if (batteryReading.percent != previous.percent ||
            batteryReading.charging != previous.charging) {
          renderNow();
        }
      }
    }
#endif
    if (quitRequested) {
      bool activationHeld = false;
      if (controller != nullptr) {
        for (int button = SDL_CONTROLLER_BUTTON_A;
             button < SDL_CONTROLLER_BUTTON_MAX;
             ++button) {
          if (SDL_GameControllerGetButton(
                  controller,
                  static_cast<SDL_GameControllerButton>(button)) != 0) {
            activationHeld = true;
            break;
          }
        }
      }
      const Uint8* keyboard = keyboardState();
      activationHeld =
          activationHeld ||
          keyboard[SDL_SCANCODE_RETURN] != 0 ||
          keyboard[SDL_SCANCODE_KP_ENTER] != 0 ||
          keyboard[SDL_SCANCODE_SPACE] != 0;
      activationHeld =
          activationHeld ||
          (SDL_GetMouseState(nullptr, nullptr) &
           (SDL_BUTTON(SDL_BUTTON_LEFT) |
            SDL_BUTTON(SDL_BUTTON_RIGHT))) != 0;
      if (!activationHeld) {
        // Keep Desktop Mode's controller-to-keyboard/mouse activation from
        // carrying into Steam or the file manager behind this window.
        SDL_Delay(350);
        running = false;
        break;
      }
    }
    if (desktopStatus[0] != '\0' &&
        SDL_TICKS_PASSED(SDL_GetTicks(), desktopStatusUntil)) {
      desktopStatus[0] = '\0';
      desktopStatusUntil = 0;
      if (currentView == DesktopView::Canvas ||
          currentView == DesktopView::Preview) {
        renderNow();
      }
    }
    if (previewLabelUntil != 0 &&
        SDL_TICKS_PASSED(SDL_GetTicks(), previewLabelUntil)) {
      previewLabelUntil = 0;
      if (currentView == DesktopView::Preview) {
        renderNow();
      }
    }
    const Uint8* heldKeys = keyboardState();
    const bool drawPressed = mouseDrawHeld ||
        heldKeys[SDL_SCANCODE_RETURN] != 0 ||
        heldKeys[SDL_SCANCODE_SPACE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_A) != 0);
    const bool erasePressed = mouseEraseHeld ||
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
    const bool toolPressed =
        (drawPressed && !desktopDrawPressed) ||
        (erasePressed && !desktopErasePressed) ||
        (fillPressed && !desktopFillPressed);
    desktopDrawPressed = drawPressed;
    desktopErasePressed = erasePressed;
    desktopFillPressed = fillPressed;
    if (toolPressed && currentView == DesktopView::Canvas) {
      rumble(0x0800, 0x1800, 24);
    }
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
#ifdef BITMAP16_STEAM_DECK
      const unsigned triggerMask =
          (leftTriggerHeld ? 1u : 0u) | (rightTriggerHeld ? 2u : 0u);
      if ((triggerMask & ~previousTriggerMask) != 0 &&
          !desktopControllerPrompts) {
        desktopControllerPrompts = true;
        if (currentView == DesktopView::Canvas) renderNow();
      }
      previousTriggerMask = triggerMask;
#endif
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
          std::snprintf(
              previewLabel,
              sizeof(previewLabel),
              "Zoom: %dx",
              bitmap16::PreviewView::resolvedZoom(
                  previewState,
                  previewSketch.gridSize,
                  std::min(width, height)));
          previewLabelUntil = SDL_GetTicks() + 2000u;
          rumble(0x0800, 0x2000, 28);
          renderNow();
        } else if (
            (leftTriggerHeld && !previewLeftTriggerLatched) ||
            (rightTriggerHeld && !previewRightTriggerLatched)) {
          rumble(0x3000, 0x0800, 70);
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
        if (currentView == DesktopView::Palette) {
          pushControllerEvent(leftX < 0 ? kColorLeft : kColorRight);
        } else {
          pushKeyEvent(leftX < 0 ? SDLK_LEFT : SDLK_RIGHT, repeat);
        }
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
        if (currentView == DesktopView::Palette) {
          pushControllerEvent(
              dpadHorizontal < 0 ? kColorLeft : kColorRight);
        } else {
          pushKeyEvent(
              dpadHorizontal < 0 ? SDLK_LEFT : SDLK_RIGHT, repeat);
        }
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
    editor.setHeldActions(
        currentView == DesktopView::Canvas && desktopDrawPressed,
        currentView == DesktopView::Canvas && desktopErasePressed,
        currentView == DesktopView::Canvas &&
            (heldKeys[SDL_SCANCODE_M] != 0 || controllerMoveHeld));
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    // Sensor polling must run even when no key arrives or the queue stays busy.
    const bool shakeEnabled = currentView == DesktopView::Canvas &&
        settings.shakeUndoEnabled;
    if (zeroImu.pollShake(SDL_GetTicks(), shakeEnabled) && editor.undo()) {
      setDesktopStatus("Undo");
      renderNow();
    }
#endif
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
    int eventWait = animatedView ? animationWait : -1;
    if (currentView == DesktopView::Palette) {
      // Rendering/output time counts toward the frame budget, rather than
      // always adding another 16 ms after a frame has already been drawn.
      const Uint32 elapsed = SDL_GetTicks() - paletteFrameAt;
      eventWait = elapsed >= paletteFrameInterval ? 0 : static_cast<int>(paletteFrameInterval - elapsed);
      if (paletteRenderPending) {
        const Uint32 sincePresent = SDL_GetTicks() - palettePresentedAt;
        const int remaining = sincePresent >= paletteFrameInterval
            ? 0 : static_cast<int>(paletteFrameInterval - sincePresent);
        eventWait = std::min(eventWait, remaining);
      }
    }
#ifdef BITMAP16_LOSPEC
    if (paletteBrowser.pending()) eventWait = eventWait < 0 ? 16 : std::min(16, eventWait);
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    const int sensorWait = zeroImu.waitTimeout(SDL_GetTicks(), shakeEnabled);
    if (sensorWait >= 0) {
      eventWait = eventWait < 0 ? sensorWait : std::min(eventWait, sensorWait);
    }
    if ((currentView == DesktopView::Canvas && settings.batteryDisplay) ||
        currentView == DesktopView::Charging) {
      const Uint32 now = SDL_GetTicks();
      const int remaining = SDL_TICKS_PASSED(now, nextBatteryRefresh)
          ? 0
          : static_cast<int>(nextBatteryRefresh - now);
      eventWait = eventWait < 0 ? remaining : std::min(eventWait, remaining);
    }
#endif
#ifdef BITMAP16_CARDPUTER_ZERO
    const bool canvasStatus =
        currentView == DesktopView::Canvas && desktopStatus[0] != '\0';
    const bool previewStatus =
        currentView == DesktopView::Preview && previewLabelUntil != 0;
    if (canvasStatus || previewStatus) {
      const Uint32 deadline =
          canvasStatus ? desktopStatusUntil : previewLabelUntil;
      const Uint32 now = SDL_GetTicks();
      const int remaining = SDL_TICKS_PASSED(now, deadline)
          ? 0
          : static_cast<int>(deadline - now);
      eventWait = eventWait < 0 ? remaining : std::min(eventWait, remaining);
    }
#endif
    const int eventAvailable = waitEvent(&event, eventWait);
#ifdef BITMAP16_CARDPUTER_ZERO
    if (currentView == DesktopView::Help &&
        helpState.externalKeyboard != externalKeyboardActive()) renderNow();
#endif
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    if (zeroHomeRequested) break;
#endif
    if (eventAvailable && event.type == musicEventType) {
      renderNow();
      continue;
    }
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

#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    if (nativeIo.isBacklightEvent(event)) {
      char message[32] = {};
      std::snprintf(message, sizeof(message), "Brightness: %d%%",
                    event.user.code);
      if (currentView == DesktopView::Canvas) {
        setDesktopStatus(message);
        renderNow();
      } else if (currentView == DesktopView::Preview) {
        setPreviewLabel(message);
        renderNow();
      }
      continue;
    }
#endif

#ifdef BITMAP16_STEAM_DECK
    // Controller-generated key events have no window ID. Inspect the original
    // event before mouse clicks or controller buttons are mapped to shortcuts.
    bool controllerPrompts = desktopControllerPrompts;
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
      controllerPrompts = event.key.windowID == 0;
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN ||
               event.type == SDL_USEREVENT) {
      controllerPrompts = true;
    } else if (event.type == SDL_KEYDOWN && event.key.windowID == 0) {
      controllerPrompts = true;
    }
    if (controllerPrompts != desktopControllerPrompts) {
      desktopControllerPrompts = controllerPrompts;
      if (currentView == DesktopView::Canvas) renderNow();
    }
#endif
    if ((event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
         event.type == SDL_MOUSEBUTTONUP) &&
        (event.type == SDL_MOUSEMOTION ? event.motion.windowID : event.button.windowID) ==
            SDL_GetWindowID(window) && currentView == DesktopView::Canvas) {
      const bool motion = event.type == SDL_MOUSEMOTION;
      const int pointerX = motion ? event.motion.x : event.button.x;
      const int pointerY = motion ? event.motion.y : event.button.y;
      uint8_t cellX = 0, cellY = 0;
      const bool inside = bitmap16::CanvasView::cellAtPointer(
          width, height, editor.sketch().gridSize, desktopCanvasViewport,
          pointerX, pointerY, cellX, cellY);
      if (event.type == SDL_MOUSEBUTTONDOWN && inside) {
        if (event.button.button == SDL_BUTTON_LEFT) mouseDrawHeld = true;
        if (event.button.button == SDL_BUTTON_RIGHT) mouseEraseHeld = true;
      } else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT) mouseDrawHeld = false;
        if (event.button.button == SDL_BUTTON_RIGHT) mouseEraseHeld = false;
      }
      const Uint8* pointerKeys = keyboardState();
      const bool pointerDrawHeld = mouseDrawHeld ||
          pointerKeys[SDL_SCANCODE_RETURN] != 0 ||
          pointerKeys[SDL_SCANCODE_SPACE] != 0 ||
          (controller != nullptr && SDL_GameControllerGetButton(
              controller, SDL_CONTROLLER_BUTTON_A) != 0);
      const bool pointerEraseHeld = mouseEraseHeld ||
          pointerKeys[SDL_SCANCODE_BACKSPACE] != 0 ||
#ifndef BITMAP16_CARDPUTER_ZERO
          pointerKeys[SDL_SCANCODE_DELETE] != 0 ||
#endif
          (controller != nullptr && SDL_GameControllerGetButton(
              controller, SDL_CONTROLLER_BUTTON_X) != 0);
      editor.setHeldActions(pointerDrawHeld, pointerEraseHeld,
                           pointerKeys[SDL_SCANCODE_M] != 0 || controllerMoveHeld);
      desktopDrawPressed = pointerDrawHeld;
      desktopErasePressed = pointerEraseHeld;
      if (inside && (motion || (event.type == SDL_MOUSEBUTTONDOWN &&
                      (event.button.button == SDL_BUTTON_LEFT ||
                       event.button.button == SDL_BUTTON_RIGHT)))) {
        if (pointerDrawHeld || pointerEraseHeld) {
          editor.paintTo(cellX, cellY, pointerEraseHeld, motion && pointerInsideCanvas);
        } else {
          editor.setCursor(cellX, cellY);
        }
        renderNow();
      } else if (event.type == SDL_MOUSEBUTTONUP) {
        renderNow();
      }
      pointerInsideCanvas = inside;
      // Leave the existing Palettes-button handler available for UI clicks.
      if (motion || event.type == SDL_MOUSEBUTTONUP || inside) continue;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT &&
        event.button.windowID == SDL_GetWindowID(window) &&
        currentView == DesktopView::Canvas &&
        bitmap16::CanvasView::paletteButtonContains(
            width, height, editor.sketch().gridSize,
            event.button.x, event.button.y)) {
      // SDL maps mouse coordinates to the renderer's logical framebuffer.
      // Use the shortcut path so every activation opens the same palette view.
      event = {};
      event.type = SDL_KEYDOWN;
      event.key.keysym.sym = SDLK_p;
    }
    if (event.type == SDL_QUIT) {
      if (requestSketchAction(SketchAction::Quit)) running = false;
      continue;
    }
    if (event.type == SDL_WINDOWEVENT &&
        event.window.event == SDL_WINDOWEVENT_CLOSE &&
        event.window.windowID == SDL_GetWindowID(window)) {
      if (requestSketchAction(SketchAction::Quit)) running = false;
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
#ifdef BITMAP16_STEAM_DECK
      if (currentView == DesktopView::Help &&
          (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ||
           event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
        const bool previous = event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
        if (previous) controllerUndoReleaseConsumed = true;
        if (bitmap16::HelpView::moveGroup(helpState, previous ? -1 : 1, false, false)) {
          audioPlayer.playNavigationTick();
          renderNow();
        }
        continue;
      }
#endif
#ifdef BITMAP16_LOSPEC
      if (currentView == DesktopView::Palette) {
        SDL_Keycode paletteKey = SDLK_UNKNOWN;
#if defined(BITMAP16_STEAM_DECK) && SDL_VERSION_ATLEAST(2, 0, 14)
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_PADDLE2) paletteKey = SDLK_i;
#elif !defined(BITMAP16_STEAM_DECK)
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK) paletteKey = SDLK_i;
#endif
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_Y) paletteKey = SDLK_l;
        if (paletteBrowser.online()) {
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_X) paletteKey = SDLK_r;
        }
#ifdef BITMAP16_STEAM_DECK
        if (paletteKey == SDLK_UNKNOWN)
          paletteKey = paletteFilterKey(event.cbutton.button, paletteState.filterSize, !paletteBrowser.online());
#endif
        if (paletteKey != SDLK_UNKNOWN) {
          if (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
            controllerUndoReleaseConsumed = true;
          pushKeyEvent(paletteKey, false);
          continue;
        }
      }
#endif
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
          mappedKey =
              currentView == DesktopView::Memory ? SDLK_y : SDLK_f;
          break;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
          mappedKey = SDLK_p;
          break;
#ifndef BITMAP16_STEAM_DECK
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
          mappedKey = SDLK_g;
          break;
#endif
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
          pushControllerEvent(kRedo);
          break;
        case SDL_CONTROLLER_BUTTON_BACK:
          mappedKey = SDLK_o;
          break;
        case SDL_CONTROLLER_BUTTON_START:
          pushControllerEvent(kToggleSettings);
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
      if (controllerUndoReleaseConsumed) {
        controllerUndoReleaseConsumed = false;
        continue;
      }
      if (currentView == DesktopView::Palette) continue;
      pushKeyEvent(SDLK_z, false);
      continue;
    }
    if (event.type == SDL_USEREVENT && event.user.code == kRedo) {
      if (currentView == DesktopView::Canvas) {
        const bool redone = editor.redo();
        setDesktopStatus(redone ? "Redo" : "No redo");
        rumble(
            redone ? 0x0800 : 0x3800,
            redone ? 0x1800 : 0x0800,
            redone ? 30 : 110);
        renderNow();
      }
      continue;
    }
    if (event.type == SDL_USEREVENT &&
        event.user.code == kToggleSettings) {
      currentView = currentView == DesktopView::Settings
          ? DesktopView::Canvas
          : DesktopView::Settings;
      renderNow();
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
      if (changed && currentView == DesktopView::Palette) {
        rumble(0x0400, 0x1400, 22);
      }
      if (changed) {
        audioPlayer.playNavigationTick();
        renderNow();
      }
      continue;
    }
    if (event.type == SDL_KEYUP) {
      const SDL_Keycode released = event.key.keysym.sym;
      if (released == SDLK_RETURN || released == SDLK_SPACE) {
        editor.endUndoGroup(bitmap16::SketchHistory::Action::Draw);
      } else if (released == SDLK_BACKSPACE || released == SDLK_DELETE) {
        editor.endUndoGroup(bitmap16::SketchHistory::Action::Erase);
      } else if (released == SDLK_m) {
        editor.endUndoGroup(bitmap16::SketchHistory::Action::Move);
      }
    } else if (event.type == SDL_CONTROLLERBUTTONUP) {
      if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
        editor.endUndoGroup(bitmap16::SketchHistory::Action::Draw);
      } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
        editor.endUndoGroup(bitmap16::SketchHistory::Action::Erase);
      }
    } else if (event.type == SDL_WINDOWEVENT &&
               event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
      mouseDrawHeld = mouseEraseHeld = pointerInsideCanvas = false;
      editor.endUndoGroup();
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

    const auto navigationView = currentView;
    const int previousHelpCursor = helpState.cursor;
    const int previousSettingsCursor = settingsState.cursor;
    const auto previousSettingsPage = settingsState.page;
    const int previousMemoryCursor = memoryState.cursor;
    const int previousPaletteCursor = paletteState.cursor;
    const int previousGalleryIndex = galleryIndex;
    const auto previousColor = editor.selectedColor();
    SDL_Keycode key = event.key.keysym.sym;
    const bool altHeld = (event.key.keysym.mod & KMOD_ALT) != 0;
#ifdef BITMAP16_CARDPUTER_ZERO
    const bool ctrlHeld = (event.key.keysym.mod & KMOD_CTRL) != 0;
    const bool shiftHeld = (event.key.keysym.mod & KMOD_SHIFT) != 0;
#endif
    bool zeroEdgeJump = false;
#ifdef BITMAP16_CARDPUTER_ZERO
    key = bitmap16::Zero::navigationKey(event, currentView == DesktopView::Canvas);
    if (key == SDLK_HELP) key = SDLK_h;
#endif
    const bool arrow =
        key == SDLK_UP || key == SDLK_DOWN ||
        key == SDLK_LEFT || key == SDLK_RIGHT;
#ifdef BITMAP16_CARDPUTER_ZERO
    zeroEdgeJump = zeroEdgeJump ||
        (arrow && shiftHeld && !ctrlHeld && !altHeld && currentView == DesktopView::Canvas);
#endif
    const bool paletteControllerHorizontalHeld =
        currentView == DesktopView::Palette &&
        controller != nullptr &&
        (stickDirection(SDL_GameControllerGetAxis(
             controller, SDL_CONTROLLER_AXIS_LEFTX)) != 0 ||
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0 ||
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0);
    if (event.key.repeat != 0 && !arrow) continue;
    const Uint8* keyboard = keyboardState();
    const bool moveHeld =
        keyboard[SDL_SCANCODE_M] != 0 || controllerMoveHeld;
#ifndef BITMAP16_CARDPUTER_ZERO_DEVICE
    const bool brightnessHeld = keyboard[SDL_SCANCODE_B] != 0;
#endif
    const bool matrixHeld = platformHasLedMatrixControls() && keyboard[SDL_SCANCODE_L] != 0;
    const bool drawHeld = mouseDrawHeld ||
        keyboard[SDL_SCANCODE_RETURN] != 0 ||
        keyboard[SDL_SCANCODE_SPACE] != 0 ||
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_A) != 0);
    const bool eraseHeld = mouseEraseHeld ||
        keyboard[SDL_SCANCODE_BACKSPACE] != 0 ||
#ifndef BITMAP16_CARDPUTER_ZERO
        keyboard[SDL_SCANCODE_DELETE] != 0 ||
#endif
        (controller != nullptr &&
         SDL_GameControllerGetButton(
             controller, SDL_CONTROLLER_BUTTON_X) != 0);
    editor.setHeldActions(drawHeld, eraseHeld, moveHeld);
    if (currentView == DesktopView::Palette && paletteBrowser.closing())
      continue;
    const bool plusKey =
        key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS;
    const bool minusKey = key == SDLK_MINUS || key == SDLK_KP_MINUS;
#ifdef BITMAP16_CARDPUTER_ZERO
    const bool zoomIn = plusKey || key == SDLK_PAGEUP;
    const bool zoomOut = minusKey || key == SDLK_PAGEDOWN;
#else
    const bool zoomIn = plusKey;
    const bool zoomOut = minusKey;
#endif
    bool changed = false;
    const auto moveCanvasCursor = [&](int dx, int dy) {
      if (moveHeld) {
        return editor.shift(dx, dy);
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
    const auto jumpCanvasCursor = [&](int dx, int dy) {
      const bitmap16::CanvasView::Layout layout =
          bitmap16::CanvasView::layoutFor(
              width, height, editor.sketch().gridSize);
      const int cellSize = desktopCanvasViewport.cellSize == 0
          ? layout.cellSize
          : desktopCanvasViewport.cellSize;
      if (cellSize <= layout.cellSize) return false;
      const uint8_t oldX = editor.cursorX();
      const uint8_t oldY = editor.cursorY();
      const uint8_t edge = editor.sketch().gridSize - 1;
      editor.setCursor(
          dx < 0 ? 0 : dx > 0 ? edge : oldX,
          dy < 0 ? 0 : dy > 0 ? edge : oldY);
      bitmap16::CanvasView::keepCursorVisible(
          desktopCanvasViewport,
          width,
          height,
          editor.sketch().gridSize,
          editor.cursorX(),
          editor.cursorY());
      return oldX != editor.cursorX() || oldY != editor.cursorY();
    };

    if (key == SDLK_F12) {
#ifdef BITMAP16_STEAM_DECK
      const bitmap16::Sketch& screenshotSketch =
          currentView == DesktopView::Preview &&
                  previewOverride != nullptr
              ? *previewOverride
              : editor.sketch();
      const uint16_t screenshotBackground =
          currentView == DesktopView::Preview
              ? bitmap16::PreviewView::backgroundColor(
                    previewState, previewTheme())
              : canvasTheme(settings).background;
      std::filesystem::path outputPath;
      if (workspace.exportSteamScreenshot(
              screenshotSketch,
              screenshotBackground,
              outputPath)) {
        SDL_Log(
            "UI-free screenshot: %s",
            outputPath.string().c_str());
        setDesktopStatus("Screenshot saved");
        rumble(0x1000, 0x3800, 75);
      } else {
        SDL_Log("UI-free screenshot export failed");
        setDesktopStatus("Screenshot failed");
        rumble(0x4800, 0x0800, 180);
      }
      changed = true;
#endif
#ifdef BITMAP16_LOSPEC
    } else if (currentView == DesktopView::Palette && !paletteState.insertionAnimating &&
               (key == SDLK_i
#ifdef BITMAP16_STEAM_DECK
                || key == SDLK_g  // L4's keyboard binding on Steam Input layouts.
#endif
               )) {
      if (paletteBrowser.pending()) {
        paletteBrowser.importPalette(lastPaletteLookup);
      } else if (enterPaletteLookup(lastPaletteLookup)) {
        paletteBrowser.importPalette(lastPaletteLookup);
      }
      changed = true;
    } else if (currentView == DesktopView::Palette && !paletteBrowser.online() &&
               !paletteState.insertionAnimating &&
               (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
      paletteBrowser.deleteSelection();
      changed = true;
    } else if (currentView == DesktopView::Palette && paletteBrowser.online() &&
               !paletteState.insertionAnimating &&
               (key == SDLK_0 || key == SDLK_4 || key == SDLK_8 || key == SDLK_1)) {
      pendingPaletteSize = key == SDLK_0 ? 0 : key == SDLK_4 ? 4 : key == SDLK_8 ? 8 : 16;
      requestPaletteExit(PaletteExitAction::FilterSize);
      changed = true;
    } else if (currentView == DesktopView::Palette &&
               !paletteState.insertionAnimating && (key == SDLK_b || key == SDLK_l)) {
      requestPaletteExit(PaletteExitAction::SwitchList);
      changed = true;
    } else if (currentView == DesktopView::Palette && paletteBrowser.online() &&
               !paletteState.insertionAnimating && key == SDLK_r) {
      paletteBrowser.refresh(SDL_GetTicks());
      changed = true;
#endif
    } else if (key == SDLK_m && currentView == DesktopView::Canvas) {
      desktopMoveModeActive = true;
      changed = true;
    } else if (key == SDLK_ESCAPE) {
      if (event.type == SDL_KEYDOWN && event.key.repeat != 0) continue;
      if (currentView == DesktopView::Palette) {
#ifdef BITMAP16_LOSPEC
        requestPaletteExit(paletteBrowser.online()
            ? PaletteExitAction::SwitchList : PaletteExitAction::Close);
#else
        requestPaletteExit(PaletteExitAction::Close);
#endif
        changed = true;
        continue;
      }
      if (currentView == DesktopView::Preview && galleryMode) {
        currentView = DesktopView::Memory;
        memoryState.cursor = galleryIndex + 1;
        galleryMode = false;
        galleryAutoAdvance = false;
        previewOverride = nullptr;
      } else if (currentView == DesktopView::Help) {
        currentView = returnView;
        previewOverride = nullptr;
      } else if (
          currentView == DesktopView::Settings &&
          (settingsState.page == bitmap16::SettingsView::Page::RgbMatrix ||
           settingsState.page == bitmap16::SettingsView::Page::Audio)) {
        const int parentCursor = settingsState.page == bitmap16::SettingsView::Page::Audio
            ? settingsState.parentCursor : 2;
        settingsState.page = bitmap16::SettingsView::Page::Main;
        settingsState.cursor = parentCursor;
        settingsState.scrollOffset = 0;
      } else if (currentView != DesktopView::Canvas) {
        currentView = DesktopView::Canvas;
        previewOverride = nullptr;
      }
      changed = true;
#ifndef BITMAP16_CARDPUTER_ZERO
    } else if (key == SDLK_q) {
      if (requestSketchAction(SketchAction::Quit)) running = false;
#endif
    } else if (currentView == DesktopView::Charging) {
      currentView = DesktopView::Canvas;
      changed = true;
#ifndef BITMAP16_CARDPUTER_ZERO_DEVICE
    } else if (brightnessHeld && (plusKey || minusKey)) {
      adjustDisplayBrightness(plusKey ? 5 : -5);
      changed = true;
#endif
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
#ifndef BITMAP16_STEAM_DECK
    } else if (
        currentView == DesktopView::Canvas &&
        (zoomIn || zoomOut)) {
      changed = bitmap16::CanvasView::adjustZoom(
          desktopCanvasViewport,
          zoomIn ? 1 : -1,
          width,
          height,
          editor.sketch().gridSize,
          editor.cursorX(),
          editor.cursorY());
      if (changed) {
        const bitmap16::CanvasView::Layout zoomLayout =
            bitmap16::CanvasView::layoutFor(
                width, height, editor.sketch().gridSize);
        const int zoomCellSize =
            desktopCanvasViewport.cellSize == 0
                ? zoomLayout.cellSize
                : desktopCanvasViewport.cellSize;
        char zoomStatus[16] = {};
        std::snprintf(
            zoomStatus,
            sizeof(zoomStatus),
            "Zoom: %dx",
            zoomCellSize / zoomLayout.cellSize);
        setDesktopStatus(zoomStatus);
      }
#endif
    } else if (
        currentView == DesktopView::Preview &&
        (zoomIn || zoomOut)) {
      const bitmap16::Sketch& previewSketch =
          previewOverride == nullptr ? editor.sketch() : *previewOverride;
      changed = bitmap16::PreviewView::adjustZoom(
          previewState,
          zoomIn ? 1 : -1,
          previewSketch.gridSize,
          std::min(width, height));
      if (changed) {
        std::snprintf(
            previewLabel,
            sizeof(previewLabel),
            "Zoom: %dx",
            bitmap16::PreviewView::resolvedZoom(
                previewState,
                previewSketch.gridSize,
                std::min(width, height)));
        previewLabelUntil = SDL_GetTicks() + 2000u;
        rumble(0x0800, 0x2000, 28);
      } else {
        rumble(0x3000, 0x0800, 70);
      }
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
    } else if (
        key == SDLK_t &&
        !(controller != nullptr &&
          SDL_GameControllerGetButton(
              controller, SDL_CONTROLLER_BUTTON_START) != 0)) {
      currentView = currentView == DesktopView::Settings
          ? DesktopView::Canvas
          : DesktopView::Settings;
      changed = true;
    } else if (key == SDLK_v) {
      if (currentView == DesktopView::Preview) {
        previewLabelUntil = 0;
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
        setPreviewLabel("Preview");
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
#ifdef BITMAP16_STEAM_DECK
        {
          const bitmap16::Sketch& previewSketch =
              previewOverride == nullptr ? editor.sketch() : *previewOverride;
          const int logicalPreviewSize =
              std::min(128, std::min(width, height));
          previewState.zoom = static_cast<uint8_t>(
              std::max(1, logicalPreviewSize / previewSketch.gridSize));
        }
#endif
        currentView = DesktopView::Preview;
      }
      changed = true;
    } else if (
        key == SDLK_b &&
#ifdef BITMAP16_CARDPUTER_ZERO
        ctrlHeld &&
#else
        altHeld &&
#endif
        platformHasBatteryDisplay()) {
      currentView = DesktopView::Charging;
      changed = true;
    } else if (
        key == SDLK_p &&
        (currentView == DesktopView::Canvas ||
         currentView == DesktopView::Palette)) {
      if (currentView == DesktopView::Palette) {
        requestPaletteExit(PaletteExitAction::Close);
      } else {
        if (bitmap16::CanvasView::hasPaletteButton(width, height)) {
          desktopPalettePressed = true;
          renderNow();
          desktopPalettePressed = false;
        }
        paletteBrowser.open();
        currentView = DesktopView::Palette;
      }
      changed = true;
    } else if (key == SDLK_o) {
      if (currentView == DesktopView::Memory) {
        currentView = DesktopView::Canvas;
      } else {
        refreshMemoryCatalog();
        if (focusNewestSketchOnNextMemoryOpen &&
            !workspace.sketches().empty()) {
          memoryState.cursor = 1;
          memoryState.scrollOffset = 0;
          memoryState.scrollPosition = 0.0f;
          focusNewestSketchOnNextMemoryOpen = false;
        }
        currentView = DesktopView::Memory;
      }
      changed = true;
    } else if (key == SDLK_d) {
      currentView = DesktopView::Canvas;
      changed = true;
    } else if (
        currentView == DesktopView::Canvas && key == SDLK_s &&
#ifdef BITMAP16_CARDPUTER_ZERO
        (ctrlHeld || shiftHeld)
#else
        true
#endif
        ) {
#ifdef BITMAP16_CARDPUTER_ZERO
      const bool saveAsNew = shiftHeld;
#else
      const bool saveAsNew = altHeld;
#endif
      if (sketchWorkflow.save(saveAsNew)) {
        audioPlayer.playSketchSave();
        refreshMemoryCatalog();
        SDL_Log(saveAsNew ? "Saved new sketch" : "Saved sketch");
        setDesktopStatus("Saved");
        rumble(0x1000, 0x3800, 75);
        focusNewestSketchOnNextMemoryOpen = true;
        changed = true;
      } else {
        SDL_Log("Sketch save failed");
        setDesktopStatus("Failed to save");
        rumble(0x4800, 0x0800, 180);
        changed = true;
      }
    } else if (
        currentView == DesktopView::Canvas &&
#ifdef BITMAP16_CARDPUTER_ZERO
        ctrlHeld && (key == SDLK_x || key == SDLK_l)
#else
        key == SDLK_x
#endif
        ) {
#ifdef BITMAP16_CARDPUTER_ZERO
      const bool scaledExport = key == SDLK_x;
#else
      const bool scaledExport = !altHeld;
#endif
      std::filesystem::path outputPath;
      if (workspace.exportSketch(
              editor.sketch(), scaledExport, outputPath)) {
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
#ifndef BITMAP16_CARDPUTER_ZERO
    } else if (currentView == DesktopView::Canvas && key == SDLK_n) {
      if (requestSketchAction(SketchAction::NewSketch)) setDesktopStatus("New sketch");
      changed = true;
#endif
    } else if (
        currentView == DesktopView::Canvas &&
#ifdef BITMAP16_CARDPUTER_ZERO
        key == SDLK_i
#else
        key == SDLK_c
#endif
        ) {
      const uint8_t next = static_cast<uint8_t>(
          editor.selectedColor() % editor.sketch().paletteSize + 1);
      editor.setSelectedColor(next);
      changed = true;
    } else if (
        currentView == DesktopView::Canvas &&
#ifdef BITMAP16_CARDPUTER_ZERO
        ((key == SDLK_f && ctrlHeld) || key == SDLK_e)
#else
        key == SDLK_f
#endif
        ) {
#ifdef BITMAP16_CARDPUTER_ZERO
      const bool eraseFill = key == SDLK_e;
#else
      const bool eraseFill = altHeld;
#endif
      editor.floodFill(eraseFill ? 0 : editor.selectedColor());
      setDesktopStatus(eraseFill ? "Erase Fill" : "Fill");
      changed = true;
    } else if (
        currentView == DesktopView::Canvas &&
#ifdef BITMAP16_CARDPUTER_ZERO
        ctrlHeld && (key == SDLK_z || key == SDLK_y)
#else
        key == SDLK_z
#endif
        ) {
#ifdef BITMAP16_CARDPUTER_ZERO
      const bool redo = key == SDLK_y || (key == SDLK_z && shiftHeld);
#else
      const bool redo = altHeld;
#endif
      const bool performed = redo ? editor.redo() : editor.undo();
      setDesktopStatus(
          redo
              ? performed ? "Redo" : "No redo"
              : performed ? "Undo" : "No undo");
      rumble(
          performed ? 0x0800 : 0x3800,
          performed ? 0x1800 : 0x0800,
          performed ? 30 : 110);
      changed = true;
#ifndef BITMAP16_CARDPUTER_ZERO_DEVICE
    } else if (
        currentView == DesktopView::Canvas && key == SDLK_y &&
        settings.shakeUndoEnabled) {
      changed = editor.undo();
#endif
    } else if (currentView == DesktopView::Canvas && key == SDLK_g) {
      if (!chooseGridSize()) {
        renderNow();
        continue;
      }
      changed = true;
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
    } else if (currentView == DesktopView::Canvas &&
#ifdef BITMAP16_CARDPUTER_ZERO
               key == SDLK_DELETE) {
      editor.clear();
      setDesktopStatus("Clear");
      changed = true;
    } else if (currentView == DesktopView::Canvas &&
               key == SDLK_BACKSPACE) {
      changed = editor.erase();
#else
               (key == SDLK_BACKSPACE || key == SDLK_DELETE)) {
      if (altHeld) {
        editor.clear();
        setDesktopStatus("Clear");
        changed = true;
      } else {
        changed = editor.erase();
      }
#endif
#ifndef BITMAP16_CARDPUTER_ZERO
    } else if (currentView == DesktopView::Canvas && key == SDLK_k) {
      editor.clear();
      setDesktopStatus("Clear");
      changed = true;
#endif
    } else if (
        currentView == DesktopView::Canvas &&
        key >= SDLK_1 && key <= SDLK_8) {
      const uint8_t color = static_cast<uint8_t>(
          key - SDLK_0 +
#ifdef BITMAP16_CARDPUTER_ZERO
          0
#else
          (altHeld ? 8 : 0)
#endif
          );
      if (color <= editor.sketch().paletteSize) {
        editor.setSelectedColor(color);
        changed = true;
      }
#ifdef BITMAP16_CARDPUTER_ZERO
    } else if (
        currentView == DesktopView::Canvas &&
        key >= SDLK_F1 && key <= SDLK_F8) {
      const uint8_t color = static_cast<uint8_t>(key - SDLK_F1 + 9);
      if (color <= editor.sketch().paletteSize) {
        editor.setSelectedColor(color);
        changed = true;
      }
#endif
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
        changed = zeroEdgeJump
            ? jumpCanvasCursor(0, -1)
            : moveCanvasCursor(0, -1);
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
            platformHasShakeUndo(),
            platformHasQuitSetting(),
            false,
            platformHasHelpSetting(), true);
      } else if (currentView == DesktopView::Memory) {
        changed = bitmap16::MemoryView::moveCursor(
            memoryState, 0, -1, memoryCatalog.count, width);
      }
    } else if (key == SDLK_DOWN) {
      if (currentView == DesktopView::Canvas) {
        changed = zeroEdgeJump
            ? jumpCanvasCursor(0, 1)
            : moveCanvasCursor(0, 1);
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
            platformHasShakeUndo(),
            platformHasQuitSetting(),
            false,
            platformHasHelpSetting(), true);
      } else if (currentView == DesktopView::Memory) {
        changed = bitmap16::MemoryView::moveCursor(
            memoryState, 0, 1, memoryCatalog.count, width);
      }
    } else if (
        currentView == DesktopView::Canvas &&
        (key == SDLK_LEFT || key == SDLK_RIGHT)) {
      const int delta = key == SDLK_LEFT ? -1 : 1;
      changed = zeroEdgeJump
          ? jumpCanvasCursor(delta, 0)
          : moveCanvasCursor(delta, 0);
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
              platformHasShakeUndo(),
              platformHasQuitSetting(),
              false,
              platformHasHelpSetting(), true, key == SDLK_LEFT ? -1 : 1);
      changed = action != bitmap16::SettingsView::Action::None;
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
      nativeIo.setDisplayWarmth(settings.displayWarmth);
#endif
      if (action == bitmap16::SettingsView::Action::SoundEffectsChanged) {
        audioPlayer.setSoundEffectsEnabled(settings.soundEffectsEnabled);
        workspace.saveSettings();
      } else if (action == bitmap16::SettingsView::Action::SoundEffectsVolumeChanged) {
        audioPlayer.setSoundEffectsVolume(settings.soundEffectsVolume);
        workspace.saveSettings();
      } else if (action == bitmap16::SettingsView::Action::MusicVolumeChanged) {
        audioPlayer.setMusicVolume(settings.musicVolume);
        workspace.saveSettings();
      } else if (action == bitmap16::SettingsView::Action::MusicEnabledChanged) {
        audioPlayer.setMusicEnabled(settings.musicEnabled);
        settings.musicEnabled = audioPlayer.musicEnabled();
        workspace.saveSettings();
      } else if (action == bitmap16::SettingsView::Action::QuitRequested) {
        if (requestSketchAction(SketchAction::Quit)) quitRequested = true;
      } else if (action == bitmap16::SettingsView::Action::HelpRequested) {
        returnView = DesktopView::Settings;
        currentView = DesktopView::Help;
      } else if (
          action ==
              bitmap16::SettingsView::Action::MatrixEnabledChanged &&
          matrixSimulator != nullptr) {
        matrixSimulator->setEnabled(settings.matrixEnabled);
        workspace.saveSettings();
      } else if (changed) {
        workspace.saveSettings();
      }
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_LEFT || key == SDLK_RIGHT) &&
        !paletteControllerHorizontalHeld) {
      changed = bitmap16::PaletteView::moveCursor(
          paletteState, key == SDLK_LEFT ? -1 : 1);
      if (changed) {
        rumble(0x0400, 0x1400, 22);
      }
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
      const bool creating = memoryState.cursor == 0;
      const std::string target = creating ? std::string{} : workspace.sketchKey(
          static_cast<std::size_t>(memoryState.cursor - 1));
      if (requestSketchAction(creating ? SketchAction::NewSketch : SketchAction::OpenSketch, target)) {
        sketchOpened = !creating;
        currentView = DesktopView::Canvas;
        setDesktopStatus(creating ? "New sketch" : "Loaded");
      }
      changed = true;
    } else if (
        currentView == DesktopView::Memory &&
#ifdef BITMAP16_CARDPUTER_ZERO
        key == SDLK_s && ctrlHeld &&
#else
        key == SDLK_y &&
#endif
        memoryState.cursor > 0) {
      const std::size_t sourceIndex =
          static_cast<std::size_t>(memoryState.cursor - 1);
      if (sourceIndex < workspace.sketches().size()) {
        if (workspace.duplicateSketch(sourceIndex)) {
          refreshMemoryCatalog();
          memoryState.cursor = 1;
          memoryState.scrollOffset = 0;
          memoryState.scrollPosition = 0.0f;
          setDesktopStatus("Duplicated");
          rumble(0x1000, 0x3800, 75);
          changed = true;
        } else {
          setDesktopStatus("Duplicate failed");
          rumble(0x4800, 0x0800, 180);
          changed = true;
        }
      }
    } else if (
        currentView == DesktopView::Memory &&
        (key == SDLK_BACKSPACE || key == SDLK_DELETE) &&
        memoryState.cursor > 0) {
      const std::size_t deleteIndex =
          static_cast<std::size_t>(memoryState.cursor - 1);
      const bool deletedActive =
          workspace.activeIndex() == static_cast<int>(deleteIndex);
      changed = confirmSketchDeletion() && workspace.deleteSketch(deleteIndex);
      if (changed) {
        if (deletedActive) sketchWorkflow.savedCopyRemoved();
        refreshMemoryCatalog();
        setDesktopStatus("Deleted sketch");
      }
    } else if (
        currentView == DesktopView::Memory &&
#ifdef BITMAP16_CARDPUTER_ZERO
        key == SDLK_u
#else
        key == SDLK_z
#endif
        ) {
      const bool wasDetached = workspace.hasDetachedSketch();
      changed = workspace.undoDelete();
      if (changed) {
        if (wasDetached && !workspace.hasDetachedSketch()) sketchWorkflow.savedCopyRestored();
        refreshMemoryCatalog();
        setDesktopStatus("Restored sketch");
      }
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
      if (paletteBrowser.select()) {
        cartridgeApplied = true;
        audioPlayer.playCartridgeSlide();
        paletteCompletionAt = 0;
        rumble(0x1800, 0x5000, 120);
        changed = true;
      } else {
        rumble(0x4800, 0x0800, 140);
        changed = true;
      }
    } else if (
        currentView == DesktopView::Palette &&
        (key == SDLK_0 || key == SDLK_4 ||
         key == SDLK_8 || key == SDLK_1)) {
      pendingPaletteSize = key == SDLK_0 ? 0 : key == SDLK_4 ? 4 : key == SDLK_8 ? 8 : 16;
      requestPaletteExit(PaletteExitAction::FilterSize);
      changed = true;
    } else if (
        currentView == DesktopView::Palette && key == SDLK_u) {
      requestPaletteExit(PaletteExitAction::FilterUser);
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
    const bool selectionChanged = navigationView == currentView && (
        (currentView == DesktopView::Canvas && editor.selectedColor() != previousColor) ||
        (currentView == DesktopView::Help && helpState.cursor != previousHelpCursor) ||
        (currentView == DesktopView::Settings && (settingsState.cursor != previousSettingsCursor ||
                                               settingsState.page != previousSettingsPage)) ||
        (currentView == DesktopView::Memory && memoryState.cursor != previousMemoryCursor) ||
        (currentView == DesktopView::Palette && paletteState.cursor != previousPaletteCursor) ||
        (currentView == DesktopView::Preview && galleryIndex != previousGalleryIndex));
    if (changed && selectionChanged) audioPlayer.playNavigationTick();
    if (changed) renderNow();
  }

  paletteBrowser.cancelAndWait();
  musicShortcut = {};
  appAudio = nullptr;
  audioPlayer.close();
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
