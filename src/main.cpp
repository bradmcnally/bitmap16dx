/**
 * BitMap16 DX - v0.7.1
 *
 * Working pixel sketch station for Cardputer ADV!
 *
 * Controls:
 * - Arrow keys (;, ., ,, /) to move cursor (hold to repeat)
 * - Number keys 1-8 select colors (1-8)
 * - Fn + Number keys select colors (9-16)
 * - C to cycle to next color
 * - Enter to place pixel with selected color
 * - Backspace to erase pixel
 * - Hold Enter + Arrow keys to draw lines
 * - Hold Backspace + Arrow keys to erase lines
 * - G to toggle between 8×8 and 16×16 grid
 * - Z to undo last action
 * - Fn+Z to redo the last undone action
 * - Shake device to undo (IMU gesture)
 * - S to save current canvas as snapshot
 * - Fn+S to save as new sketch
 * - O to open Memory View (browse/load saved snapshots)
 * - H to open Controls/Help screen
 * - T to open Settings menu (theme, RGB matrix, export, shake undo)
 * - V to view canvas (128×128, centered)
 *   - In view mode: 1=black bg, 2=white bg, 3=gray bg, 4=dark gray bg
 *   - In view mode: B + Plus/Minus to adjust brightness
 *   - In Memory View: V opens gallery preview with navigation
 *     - Left/Right arrows: navigate sketches
 *     - Space: toggle auto-advance slideshow (3 sec interval)
 *     - B + Plus/Minus: adjust brightness
 *     - V or ESC: exit back to Memory View
 * - X to export PNG (128×128 scaled)
 * - Fn+X to export PNG (logical size: 8×8 or 16×16)
#if ENABLE_SCREENSHOTS
 * - Y to take screenshot (captures full 240×135 display) [DEBUG ONLY]
#endif
 * - P to open palette menu (swap between color palettes)
 * - Hold B + press Plus (+) to increase brightness
 * - Hold B + press Minus (-) to decrease brightness
#if ENABLE_LED_MATRIX
 * - L + Enter to toggle LED matrix on/off
 * - Hold L + press Plus (+) to increase LED brightness
 * - Hold L + press Minus (-) to decrease LED brightness
#endif
 * - G0 button (physical) to clear canvas
 */

#include <M5Cardputer.h>

// Include NimBLE before PNGENC to avoid macro conflicts
#define ENABLE_BLUETOOTH 0  // DISABLED - crashes on init, debugging needed
#if ENABLE_BLUETOOTH
  #include <NimBLEDevice.h>
#endif

// Workaround for NimBLE/PNGENC 'local' macro conflict
#ifdef local
  #undef local
#endif

#include <PNGENC.h>
#include <vector>
#include <algorithm>
#include "boot_image.h"
#include "core/app.h"
#include "core/canvas_view.h"
#include "core/charging_view.h"
#include "core/help_view.h"
#include "core/led_mapping.h"
#include "core/memory_view.h"
#include "core/palette.h"
#include "core/palette_view.h"
#include "core/preview_view.h"
#include "core/settings.h"
#include "core/settings_view.h"
#include "core/shake_detector.h"
#include "core/sketch_codec.h"
#include "platform/clock.h"
#include "platform/display.h"
#include "platform/filesystem.h"
#include "platform/imu.h"
#include "platform/indicator.h"
#include "platform/input.h"
#include "platform/led_matrix.h"
#include "platform/power.h"
#include "platform/preference_store.h"

bitmap16::App app;
bitmap16::ShakeDetector shakeDetector;

// ============================================================================
// FEATURE FLAGS
// ============================================================================

// Enable screenshot feature (Y key) - disable for release builds
#define ENABLE_SCREENSHOTS 0  // Set to 0 to disable screenshots in release

// Enable external 8×8 WS2812 LED matrix support
// Set to 0 to disable LED matrix features and save memory (~9KB flash, 880 bytes RAM)
#define ENABLE_LED_MATRIX 1  // Set to 0 to disable

// Phase 4 Help-view framebuffer benchmark output on the serial monitor.
#define ENABLE_CANVAS_PROOF_TELEMETRY 1

// Macro for LED matrix canvas updates (no-op when feature disabled)
#if ENABLE_LED_MATRIX
  #define LED_CANVAS_UPDATED() canvasNeedsUpdate = true
#else
  #define LED_CANVAS_UPDATED() ((void)0)
#endif

// Note: ENABLE_BLUETOOTH is defined near top of file (before PNGENC include)
// to avoid macro conflicts. Set to 0 there to disable BT features.

// ============================================================================
// CONFIGURATION
// ============================================================================


// Firmware version displayed on boot screen
const char* FIRMWARE_VERSION = "v0.7.1";

// File format version for sketch files
// Version 1: gridSize (1B) + paletteSize (1B) + palette (32B) + pixels (256B) = 290 bytes
// Version 2: formatVersion (1B) + gridSize (1B) + paletteSize (1B) + palette (32B) + pixels (256B) = 291 bytes
const uint8_t SKETCH_FORMAT_VERSION = 2;
const int SKETCH_FILE_SIZE_V1 = 290;  // Legacy format without version byte
const int SKETCH_FILE_SIZE_V2 = 291;  // Current format with version byte

// Canvas size in logical pixels
// The canvas is always 16×16 to support both modes
const int MAX_currentGridSize = 16;

struct EditorState {
  int gridSize = 8;
  int cellSize = 16;
  int cursorX = 0;
  int cursorY = 0;
  int lastCursorScreenX = -1;
  int lastCursorScreenY = -1;
  bool moveModeActive = false;
  uint8_t canvas[16][16] = {};
  uint8_t selectedColor = 1;
  bool rulersVisible = false;
  bool drawPressed = false;
  bool erasePressed = false;
  bool fillPressed = false;
  uint8_t undoCanvas[16][16] = {};
  bool undoAvailable = false;
  uint8_t undoPaletteSize = 0;
  uint16_t undoPaletteColors[16] = {};
  uint8_t undoGridSize = 0;
  uint8_t redoCanvas[16][16] = {};
  bool redoAvailable = false;
  uint8_t redoPaletteSize = 0;
  uint16_t redoPaletteColors[16] = {};
  uint8_t redoGridSize = 0;
};

EditorState editorState;

// ============================================================================
// ICON DATA
// ============================================================================
#include "icons.h"

// ============================================================================
// CARTRIDGE GRAPHIC
// ============================================================================
// 80×92 pixel cartridge graphic for palette menu
// RGB565 format, fixed colors (not affected by palette swap)

#include "cartridge_graphic.h"

// ============================================================================
// PALETTE DEFINITIONS
// ============================================================================
#include "palettes.h"

// ============================================================================
// DYNAMIC PALETTE SYSTEM (Stock + User palettes from SD card)
// ============================================================================

// Global palette storage (combining stock + user palettes)
const uint16_t* allPalettes[32];        // Pointers (stock in PROGMEM, user in heap)
const char* allPaletteNames[32];        // Names (stock in PROGMEM, user in heap)
uint8_t allPaletteSizes[32];            // Sizes
uint8_t totalPaletteCount = NUM_PALETTES; // Start with stock palettes

// Track which palettes are user-loaded (for cleanup/memory management)
bool paletteIsUserLoaded[32] = {false};

// Palette filter state
uint8_t paletteFilterSize = 0;          // 0=all, 4=4-color, 8=8-color, 16=16-color
bool paletteFilterUser = false;         // true=show only user palettes

// Filtered palette indices (which palettes match current filter)
uint8_t filteredPaletteIndices[32];
uint8_t filteredPaletteCount = 0;

// ============================================================================
// THEME SYSTEM
// ============================================================================

// Theme color structure
struct ThemeColors {
  uint16_t background;
  uint16_t cellDark;
  uint16_t cellLight;
  uint16_t shadow;
  uint16_t text;
  uint16_t textSecondary;
  uint16_t centerLine;
  uint16_t iconDark;
  uint16_t iconLight;
};

// Light theme definition
const ThemeColors THEME_LIGHT = {
  RGB565(0xD3, 0xD3, 0xDD),  // background #d3d3dd
  RGB565(0xEE, 0xEF, 0xF4),  // cellDark #EEEFF4
  RGB565(0xFC, 0xFD, 0xFF),  // cellLight #FCFDFF
  RGB565(0xC1, 0xC4, 0xD6),  // shadow #c1c4d6
  TFT_BLACK,                 // text #000000
  RGB565(0x96, 0x94, 0x9B),  // textSecondary #4c4b4f
  RGB565(0xD3, 0xD3, 0xDD),  // centerLine (same as background)
  TFT_BLACK,                 // iconDark #000000
  TFT_WHITE                  // iconLight #ffffff
};

// Dark theme definition
const ThemeColors THEME_DARK = {
  RGB565(0x0e, 0x0e, 0x0e),  // background #0e0e0e
  RGB565(0x17, 0x17, 0x17),  // cellDark #171717
  RGB565(0x2c, 0x2c, 0x2c),  // cellLight #2c2c2c
  RGB565(0x05, 0x05, 0x05),  // shadow #050505
  TFT_WHITE,                 // text #ffffff
  RGB565(0x96, 0x94, 0x9B),  // textSecondary #4c4b4f
  RGB565(0x0e, 0x0e, 0x0e),  // centerLine (same as background)
  TFT_BLACK,                 // iconDark #000000
  RGB565(0xD3, 0xD3, 0xDD)   // iconLight #d3d3dd
};

// Active theme pointer (default to light)
const ThemeColors* currentTheme = &THEME_LIGHT;

// View mode background colors (theme-independent)
const uint16_t VIEW_BG_BLACK = TFT_BLACK;
const uint16_t VIEW_BG_WHITE = TFT_WHITE;
const uint16_t VIEW_BG_GRAY = THEME_LIGHT.background;  // Light mode background
const uint16_t VIEW_BG_DARK = THEME_DARK.background;   // Dark mode background

// ============================================================================
// STATE
// ============================================================================

// Bluetooth keeps a separate repeat state until its disabled legacy path is
// folded into the platform input adapter.
#if ENABLE_BLUETOOTH
unsigned long lastKeyTime = 0;        // When the last key action happened
unsigned long keyRepeatDelay = 300;   // Initial delay before repeat starts (ms)
unsigned long keyRepeatRate = 100;    // Time between repeats (ms)
bool keyRepeating = false;            // Whether we're in repeat mode
char lastKey = 0;                     // Track which arrow key is held
#endif

// Display brightness level (stored as percentage: 10-100%)
// Converted to hardware range (0-255) when setting display
uint8_t displayBrightness = 80;  // Start at 80% brightness

#if ENABLE_LED_MATRIX
// ============================================================================
// LED MATRIX CONFIGURATION (8×8 WS2812 RGB LEDs)
// ============================================================================

// WS2812E RGB LED matrix (8×8 or 16×16)
// Supports 1 Puzzle Unit (64 LEDs) or 4 Units (256 LEDs)
// Mirrors the canvas in real-time when enabled
#define DEFAULT_LED_BRIGHTNESS 5      // 5% brightness default
#define MIN_LED_BRIGHTNESS 1          // Minimum 1% (very dim)
#define MAX_LED_BRIGHTNESS 20         // Maximum 20% (for battery safety)

uint8_t ledBrightness = DEFAULT_LED_BRIGHTNESS;  // 1-20%
bool canvasNeedsUpdate = false;       // Flag to trigger LED update
#endif // ENABLE_LED_MATRIX

#if ENABLE_BLUETOOTH
// ============================================================================
// BLUETOOTH KEYBOARD SUPPORT
// ============================================================================
// Connects to external BLE HID keyboards for wireless input
// Uses NimBLE library for low-memory BLE stack

// Connection state
bool btEnabled = false;               // User preference (persistent)
bool btConnected = false;             // Currently connected to a keyboard
bool btScanning = false;              // Scan in progress
NimBLEClient* btClient = nullptr;
NimBLEAdvertisedDevice* btAdvDevice = nullptr;

// Bonded device for auto-reconnect
uint8_t btBondedAddr[6] = {0};        // Stored MAC address
bool btHasBondedDevice = false;

// Input state (updated from HID reports)
bool btArrowUp = false;
bool btArrowDown = false;
bool btArrowLeft = false;
bool btArrowRight = false;
bool btEnter = false;
bool btBackspace = false;
bool btEscape = false;
bool btSpace = false;                 // Space key for drawing (BT only)
bool btFill = false;
bool btFnHeld = false;                // Alt key maps to Fn

// Character queue for letters/numbers
#define BT_QUEUE_SIZE 16
char btInputQueue[BT_QUEUE_SIZE];
uint8_t btQueueHead = 0;
uint8_t btQueueTail = 0;

// HID report tracking
uint8_t btPrevReport[8] = {0};

// Last scan results (for debugging)
int btLastScanCount = 0;
int btScanCountdown = 0;  // Countdown timer during scan

// HID keycodes
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_SPACE       0x2C
#define HID_KEY_F           0x09
#define HID_KEY_RIGHT_ARROW 0x4F
#define HID_KEY_LEFT_ARROW  0x50
#define HID_KEY_DOWN_ARROW  0x51
#define HID_KEY_UP_ARROW    0x52
#define HID_KEY_LEFT_ALT    0xE2
#define HID_KEY_RIGHT_ALT   0xE6

// Notification message timing
unsigned long btNotifyTime = 0;
const unsigned long BT_NOTIFY_DURATION = 1500;  // 1.5 seconds
char btNotifyMsg[32] = "";
#endif // ENABLE_BLUETOOTH

// ============================================================================
// SKETCH SYSTEM
// ============================================================================
// Each sketch is a single drawing document with its own palette.
// Index 0 is always Transparent. Indices 1..paletteSize map to drawable colors.
// Palette changes are explicit and never rewrite pixel indices.

// Sketch data structure (unchanged format - 290 bytes on disk)
struct Sketch {
  uint8_t pixels[16][16];        // Indexed bitmap (values are palette indices)
  uint8_t gridSize;              // 8 or 16
  uint8_t paletteSize;           // 8 or 16 (number of drawable colors, excludes 0)
  uint16_t paletteColors[16];    // Maps indices 1..paletteSize to RGB565 colors
                                 // paletteColors[0] is unused (index 0 = Transparent)
  bool isEmpty;                  // Is this sketch empty?
};

struct DocumentState {
  Sketch sketch;
  bool isNew = true;
  String filename;
};

DocumentState documentState;

// Dynamic sketch list for memory view
struct SketchInfo {
  String filename;                       // e.g., "sketch_1737849600.dat"
  unsigned long timestamp;               // Unix timestamp from filename
  Sketch sketchData;                     // Cached sketch data (loaded once when entering memory view)
  bool dataLoaded;                       // Whether sketchData is valid
};

std::vector<SketchInfo> sketchList;      // Populated when entering memory view

struct CanvasProofMetrics {
  uint32_t allocationMicros = 0;
  uint32_t heapBeforeAllocation = 0;
  uint32_t heapAfterAllocation = 0;
  uint32_t lastRenderMicros = 0;
  uint32_t maxRenderMicros = 0;
  uint32_t lastBlitMicros = 0;
  uint32_t maxBlitMicros = 0;
  uint32_t minimumFreeHeap = UINT32_MAX;
  uint32_t frameCount = 0;
};

struct ViewState {
  struct {
    bitmap16::MemoryView::State navigation;
    bool canvasAvailable = false;
    bool ownsCanvas = false;
    unsigned long lastAnimationTime = 0;
  } memory;
  struct {
    bitmap16::HelpView::State navigation;
    bool canvasAvailable = false;
    bool softwareCanvas = false;
    bool ownsCanvas = false;
    CanvasProofMetrics metrics;
  } help;
  struct {
    bitmap16::PreviewView::State display;
    bool canvasAvailable = false;
    bool ownsCanvas = false;
    bool galleryMode = false;
    int galleryIndex = 0;
    unsigned long lastAdvanceTime = 0;
    bool autoAdvance = false;
  } preview;
  struct {
    bitmap16::PaletteView::State navigation;
    bool canvasAvailable = false;
    bool ownsCanvas = false;
    unsigned long lastAnimationTime = 0;
  } palette;
  struct {
    bitmap16::SettingsView::State navigation;
    bool canvasAvailable = false;
    bool ownsCanvas = false;
  } settings;
  struct {
    bitmap16::ChargingView::State animation;
    unsigned long lastFrameTime = 0;
    int batteryPercent = -1;
    unsigned long lastBatteryCheck = 0;
    Sketch sketch = {};
    bool sketchLoaded = false;
    bool canvasAvailable = false;
    bool ownsCanvas = false;
  } charging;
};

ViewState viewState;

const float MEMORY_SCROLL_SPEED = 0.35f;  // Animation speed (0.0-1.0, higher = faster)
const int MEMORY_ANIM_FRAME_MS = 16;  // Milliseconds between animation frames (16ms = 60fps - now we can afford it with caching!)

const unsigned long GALLERY_ADVANCE_INTERVAL = 3000;  // Auto-advance every 3 seconds

const float PALETTE_SCROLL_SPEED = 0.25f;  // Animation speed (0.0-1.0, higher = faster)

// Heap monitoring state
unsigned long lastHeapCheckTime = 0;
const unsigned long HEAP_CHECK_INTERVAL = 60000;  // Check every 60 seconds
const int HEAP_WARNING_THRESHOLD = 50000;  // Warn if free heap drops below 50KB
const int PALETTE_ANIM_FRAME_MS = 16;  // Milliseconds between animation frames (16ms = 60fps)

// The device loop advances about every 10 ms, so 56 steps gives an
// approximately 560 ms cartridge insertion.
const float PALETTE_INSERT_SPEED = 0.018f;
const int CHARGE_FRAME_MS = 33;  // ~30fps

// Settings preferences (loaded from NVS)
uint8_t defaultGridSize = 8;        // 8 or 16 (default grid size on boot/new sketch)
uint8_t rgbMatrixUnits = 1;         // 1 or 4 (64 or 256 LEDs)
uint8_t matrixRotation = 2;         // 0=0°, 1=90°, 2=180°, 3=270°
bool exportRGB565 = false;           // false=RGB888, true=RGB565
bool shakeUndoEnabled = false;       // true=enabled, false=disabled

// Battery display
int lastBatteryPercent = -1;  // Track last drawn battery % to avoid unnecessary redraws
unsigned long lastBatteryCheckTime = 0;  // Track when we last checked battery
const unsigned long BATTERY_CHECK_INTERVAL = 30000;  // Check battery every 30 seconds
bool batteryFirstCheck = true;  // Flag to force first battery check

bool lowBattery = false;  // Latches true when battery <= 10%

// ============================================================================
// STATUS MESSAGES - All user-facing messages in one place
// ============================================================================
// Organized by category for easy maintenance and consistency

namespace StatusMsg {
  // File Operations (SD Card)
  const char* SD_NOT_READY = "SD: Not ready";
  const char* SAVED = "Saved";
  const char* FAILED_TO_SAVE = "Failed to save";
  const char* FAILED_TO_LOAD = "Failed to load";
  const char* LOADED = "Loaded";
  const char* FILE_OPEN_FAIL = "File open fail";
  const char* WRITE_INCOMPLETE = "Write incomplete";
  const char* WRITE_FAIL = "Write fail";
  const char* FILE_NOT_FOUND = "File not found";
  const char* FILE_CORRUPT = "File corrupted";

  // Memory & Allocation
  const char* ALLOC_MEMORY = "Alloc memory...";
  const char* OUT_OF_MEMORY = "Out of memory";
  const char* LOW_MEMORY_FMT = "Low memory: %dKB";  // Format string
  const char* FREE_HEAP_FMT = "Free: %dKB";         // Format string

  // PNG Encoding
  const char* ENCODING = "Encoding...";
  const char* WRITING_FILE = "Writing file...";
  const char* WRITING = "Writing...";
  const char* PNG_ALLOC_FAIL = "PNG alloc fail";
  const char* PNG_ENCODE_FAIL = "PNG encode fail";
  const char* PNG_OPEN_ERR_FMT = "PNG open err:%d";   // Format string
  const char* PNG_INIT_ERR_FMT = "PNG init err:%d";   // Format string
  const char* ADDLINE_ERR_FMT = "addLine err:%d";     // Format string

  // Export & Screenshot
  const char* EXPORTED = "Exported!";
  const char* TOO_MANY_EXPORTS = "Too many exports";

#if ENABLE_SCREENSHOTS
  const char* SCREENSHOT = "Screenshot...";
  const char* SCREENSHOT_OK = "Screenshot OK!";
  const char* TOO_MANY_SHOTS = "Too many shots";
#endif

  // User Actions
  const char* NO_UNDO = "No undo";
  const char* UNDO = "Undo";
  const char* CLEAR = "Clear";
  const char* GRID_16X16 = "16x16";
  const char* GRID_8X8 = "8x8";
  const char* COLOR_FMT = "Color: %d";     // Format string
  const char* FILL = "Fill";
  const char* MOVE = "Move";
  const char* RESTORED_SKETCH = "Restored sketch";
}

// Debug status message
char statusMessage[32] = "";
char lastDrawnMessage[32] = "";
unsigned long statusMessageTime = 0;
const unsigned long STATUS_DISPLAY_DURATION = 2000;  // Show for 2 seconds
bool statusMessageJustCleared = false;  // Flag to trigger redraw when message expires

// SD card state
bool sdCardInitialized = false;
bool sdCardAvailable = false;

// Hardware detection (determined at boot)
const char* detectedBoardName = "Unknown";

/**
 * Set a status message to display temporarily
 */
void setStatusMessage(const char* message) {
  strncpy(statusMessage, message, sizeof(statusMessage) - 1);
  statusMessage[sizeof(statusMessage) - 1] = '\0';  // Ensure null termination
  statusMessageTime = millis();
}

// ============================================================================
// SKETCH HELPER FUNCTIONS
// ============================================================================

/**
 * Initialize the active sketch with default values
 * Sets up an empty canvas with the default SWEETIE-16 palette
 */
void initializeActiveSketch() {
  // Clear pixel data (all transparent)
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      documentState.sketch.pixels[y][x] = 0;
    }
  }

  // Use default grid size from settings, 16-color palette
  documentState.sketch.gridSize = defaultGridSize;
  documentState.sketch.paletteSize = 16;

  // Copy the default palette (first palette in catalog: SWEETIE-16)
  for (int i = 0; i < 16; i++) {
    documentState.sketch.paletteColors[i] = pgm_read_word(&allPalettes[0][i]);
  }

  documentState.sketch.isEmpty = true;
  documentState.isNew = true;
  documentState.filename = "";
}

// ============================================================================
// SD CARD FUNCTIONS
// ============================================================================

/**
 * Initialize SD card - set up SPI and mount the SD card
 * This function initializes the SD card with retry logic for reliability
 * on both M5Cardputer and M5Cardputer ADV models
 *
 * Returns true if SD card is available, false otherwise
 */
bool initSDCard() {
  if (sdCardInitialized) {
    return sdCardAvailable;
  }
  sdCardInitialized = true;
  sdCardAvailable = Filesystem::init();
  if (!sdCardAvailable) {
    return false;
  }

  Filesystem::createDirectory("/bitmap16dx");
  Filesystem::createDirectory("/bitmap16dx/sketches");
  Filesystem::createDirectory("/bitmap16dx/exports");
#if ENABLE_SCREENSHOTS
  Filesystem::createDirectory("/bitmap16dx/screenshots");
#endif
  Filesystem::createDirectory("/bitmap16dx/palettes");
  return true;
}

bitmap16::Sketch toPortableSketch(const Sketch& source) {
  bitmap16::Sketch result;
  result.gridSize = source.gridSize;
  result.paletteSize = source.paletteSize;
  result.isEmpty = source.isEmpty;
  for (int i = 0; i < 16; ++i) {
    result.paletteColors[i] = source.paletteColors[i];
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      result.pixels[y][x] = source.pixels[y][x];
    }
  }
  return result;
}

void fromPortableSketch(const bitmap16::Sketch& source, Sketch& destination) {
  destination.gridSize = source.gridSize;
  destination.paletteSize = source.paletteSize;
  destination.isEmpty = source.isEmpty;
  for (int i = 0; i < 16; ++i) {
    destination.paletteColors[i] = source.paletteColors[i];
  }
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      destination.pixels[y][x] = source.pixels[y][x];
    }
  }
}

bool readSketchFile(const char* path, Sketch& sketch) {
  uint8_t data[bitmap16::SketchCodec::kCurrentFileSize];
  std::size_t bytesRead = 0;
  if (!Filesystem::readFile(path, data, sizeof(data), bytesRead)) {
    return false;
  }

  bitmap16::Sketch decoded;
  if (bitmap16::SketchCodec::decode(data, bytesRead, decoded) !=
      bitmap16::SketchCodec::Result::Ok) {
    return false;
  }
  fromPortableSketch(decoded, sketch);
  return true;
}

bool writeSketchFile(const char* path, const Sketch& sketch) {
  uint8_t data[bitmap16::SketchCodec::kCurrentFileSize];
  std::size_t bytesWritten = 0;
  if (bitmap16::SketchCodec::encode(
          toPortableSketch(sketch),
          data,
          sizeof(data),
          bytesWritten) != bitmap16::SketchCodec::Result::Ok) {
    return false;
  }
  return Filesystem::writeFile(path, data, bytesWritten);
}

/**
 * Save a sketch to SD card
 * Path: /bitmap16dx/sketches/sketch_TIMESTAMP.dat
 *
 * File format:
 * - 1 byte: format version
 * - 1 byte: gridSize (8 or 16)
 * - 1 byte: paletteSize (8 or 16)
 * - 32 bytes: palette colors (16 colors × 2 bytes RGB565)
 * - 256 bytes: pixel data (16×16 indexed bitmap)
 * Total: 291 bytes
 *
 * Returns true if successful, false if failed
 */
/**
 * Load list of all saved sketches from SD card
 * Populates sketchList vector with sketch filenames and timestamps
 * Sorted by timestamp (newest first)
 */
bool collectSketchFile(
    const Filesystem::FileInfo& file,
    void*) {
  if (file.isDirectory) {
    return true;
  }

  String filename(file.name);
  const int lastSlash = filename.lastIndexOf('/');
  if (lastSlash >= 0) {
    filename = filename.substring(lastSlash + 1);
  }
  if (!filename.startsWith("sketch_") || !filename.endsWith(".dat")) {
    return true;
  }

  const int underscorePos = filename.indexOf('_');
  const int dotPos = filename.lastIndexOf('.');
  if (underscorePos < 0 || dotPos <= underscorePos) {
    return true;
  }

  const unsigned long timestamp =
      filename.substring(underscorePos + 1, dotPos).toInt();
  if ((file.size != SKETCH_FILE_SIZE_V1 &&
       file.size != SKETCH_FILE_SIZE_V2) ||
      timestamp == 0) {
    return true;
  }

  SketchInfo info;
  info.filename = filename;
  info.timestamp = timestamp;
  info.dataLoaded = false;
  sketchList.push_back(info);
  return true;
}

void loadSketchListFromSD() {
  sketchList.clear();

  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  if (!Filesystem::isAvailable()) {
    sdCardAvailable = false;
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  if (!Filesystem::listDirectory(
          "/bitmap16dx/sketches",
          collectSketchFile,
          nullptr)) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  // Sort by timestamp (newest first)
  std::sort(sketchList.begin(), sketchList.end(),
            [](const SketchInfo& a, const SketchInfo& b) {
              return a.timestamp > b.timestamp;
            });
}

bool findHighestSketchCounter(
    const Filesystem::FileInfo& file,
    void* context) {
  if (file.isDirectory || context == nullptr) {
    return true;
  }

  String filename(file.name);
  const int lastSlash = filename.lastIndexOf('/');
  if (lastSlash >= 0) {
    filename = filename.substring(lastSlash + 1);
  }
  if (!filename.startsWith("sketch_") || !filename.endsWith(".dat")) {
    return true;
  }

  const int dotPos = filename.lastIndexOf('.');
  const unsigned long number =
      filename.substring(7, dotPos).toInt();
  auto* highest = static_cast<unsigned long*>(context);
  if (number > *highest) {
    *highest = number;
  }
  return true;
}

/**
 * Save active sketch to SD card
 * Saves to existing file if already saved, or creates new timestamped file
 *
 * File format (290 bytes):
 * - 1 byte: gridSize (8 or 16)
 * - 1 byte: paletteSize (8 or 16)
 * - 32 bytes: palette colors (16 × 2 bytes RGB565, big endian)
 * - 256 bytes: pixel data (16×16 indexed bitmap)
 *
 * Returns true if successful, false if failed
 */
bool saveActiveSketchToSD() {
  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  if (!Filesystem::isAvailable()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    sdCardAvailable = false;
    return false;
  }

  if (!Filesystem::createDirectory("/bitmap16dx/sketches")) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    sdCardAvailable = false;
    return false;
  }

  // Generate filename (use existing if saving in place, or new timestamp if new)
  String fullPath;
  if (documentState.filename.length() > 0 && !documentState.isNew) {
    // Save to existing file
    fullPath = "/bitmap16dx/sketches/" + documentState.filename;
  } else {
    // Create new file with incrementing counter (persists across reboots)
    unsigned long counter =
        PreferenceStore::readUInt32("sketchCounter", 0);

    // If counter is 0 (first time or after NVS reset), scan existing files to find highest number
    if (counter == 0 &&
        Filesystem::exists("/bitmap16dx/sketches")) {
      Filesystem::listDirectory(
          "/bitmap16dx/sketches",
          findHighestSketchCounter,
          &counter);
    }

    counter++;
    PreferenceStore::writeUInt32("sketchCounter", counter);

    fullPath = "/bitmap16dx/sketches/sketch_" + String(counter) + ".dat";
    documentState.filename = "sketch_" + String(counter) + ".dat";

    documentState.isNew = false;
  }

  if (!writeSketchFile(fullPath.c_str(), documentState.sketch)) {
    setStatusMessage(StatusMsg::FAILED_TO_SAVE);
    sdCardAvailable = false;
    return false;
  }
  documentState.sketch.isEmpty = false;

  setStatusMessage(StatusMsg::SAVED);
  return true;
}

/**
 * Save active sketch as NEW copy (creates new timestamped file)
 * Used for Fn+S to duplicate current work
 */
bool saveActiveSketchAsNew() {
  // Force creation of new file regardless of existing filename
  documentState.filename = "";
  documentState.isNew = true;
  return saveActiveSketchToSD();
}

/**
 * Load a sketch from SD card into active sketch
 * Filename should be just the filename, not full path
 * Returns true if successful, false if failed
 */
bool loadSketchFromSD(String filename) {
  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  if (!Filesystem::isAvailable()) {
    sdCardAvailable = false;
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  String fullPath = "/bitmap16dx/sketches/" + filename;

  if (!Filesystem::exists(fullPath.c_str())) {
    setStatusMessage(StatusMsg::FILE_NOT_FOUND);
    return false;
  }

  if (!readSketchFile(fullPath.c_str(), documentState.sketch)) {
    setStatusMessage(StatusMsg::FILE_CORRUPT);
    return false;
  }

  documentState.sketch.isEmpty = false;
  documentState.filename = filename;
  documentState.isNew = false;

  return true;
}


// PNG encoding buffer - use smaller buffer to avoid memory issues
// Start with 16KB, may need to adjust based on actual compression
#define PNG_BUFFER_SIZE 16384

/**
 * Export current canvas as PNG to SD card
 *
 * @param scale If true, exports at 128×128. If false, exports at logical size (8×8 or 16×16)
 * @return true if successful, false if failed
 */
bool exportCanvasToPNG(bool scale) {
  // Ensure SD card is initialized
  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  // Check free heap before starting
  char heapMsg[40];
  snprintf(heapMsg, sizeof(heapMsg), StatusMsg::FREE_HEAP_FMT, (int)(ESP.getFreeHeap() / 1024));
  setStatusMessage(heapMsg);
  delay(500);

  setStatusMessage(StatusMsg::ALLOC_MEMORY);
  delay(50);

  // Allocate buffer for PNG output
  uint8_t* pngBuffer = (uint8_t*)malloc(PNG_BUFFER_SIZE);
  if (!pngBuffer) {
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  // Determine output size
  int outputSize = scale ? 128 : editorState.gridSize;
  int pixelScale = scale ? (128 / editorState.gridSize) : 1;

  // Allocate line buffer (RGB, 3 bytes per pixel)
  uint8_t* lineBuffer = (uint8_t*)malloc(outputSize * 3);
  if (!lineBuffer) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  setStatusMessage(StatusMsg::ENCODING);
  delay(50);

  // Allocate PNG encoder on heap (it's ~100KB so can't be on stack)
  PNGENC* png = new PNGENC();
  if (!png) {
    free(lineBuffer);
    free(pngBuffer);
    setStatusMessage(StatusMsg::PNG_ALLOC_FAIL);
    return false;
  }

  int rc = png->open(pngBuffer, PNG_BUFFER_SIZE);
  if (rc != PNG_SUCCESS) {
    delete png;
    free(lineBuffer);
    free(pngBuffer);
    char msg[40];
    snprintf(msg, sizeof(msg), StatusMsg::PNG_OPEN_ERR_FMT, rc);
    setStatusMessage(msg);
    return false;
  }

  // Reallocate line buffer for RGBA (4 bytes per pixel)
  free(lineBuffer);
  lineBuffer = (uint8_t*)malloc(outputSize * 4);
  if (!lineBuffer) {
    png->close();  // Close PNG before deletion since open() succeeded
    delete png;
    free(pngBuffer);
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  // Initialize PNG with RGBA format for transparency support (compression level 3 for lower memory usage)
  rc = png->encodeBegin(outputSize, outputSize, PNG_PIXEL_TRUECOLOR_ALPHA, 32, NULL, 3);
  if (rc != PNG_SUCCESS) {
    png->close();
    delete png;
    free(lineBuffer);
    free(pngBuffer);
    char msg[40];
    snprintf(msg, sizeof(msg), StatusMsg::PNG_INIT_ERR_FMT, rc);
    setStatusMessage(msg);
    return false;
  }

  // Write each line of the PNG
  for (int y = 0; y < outputSize; y++) {
    int canvasY = y / pixelScale;  // Map to editorState.canvas coordinate

    for (int x = 0; x < outputSize; x++) {
      int canvasX = x / pixelScale;  // Map to editorState.canvas coordinate

      // Get color index from canvas
      uint8_t colorIndex = editorState.canvas[canvasY][canvasX];
      uint8_t r, g, b, a;

      if (colorIndex == 0) {
        // Transparent pixel
        r = g = b = 0;
        a = 0;  // Fully transparent
      } else {
        // Get palette color from the active sketch's palette
        uint16_t color565 = documentState.sketch.paletteColors[colorIndex - 1];

        if (exportRGB565) {
          // Export as RGB565 (simple bit shift, faster but less accurate)
          r = ((color565 >> 11) & 0x1F) << 3;  // 5-bit red → 8-bit
          g = ((color565 >> 5) & 0x3F) << 2;   // 6-bit green → 8-bit
          b = (color565 & 0x1F) << 3;          // 5-bit blue → 8-bit
          a = 255;  // Fully opaque
        } else {
          // Export as RGB888 (using proper conversion with bit expansion)
          r = ((color565 >> 11) & 0x1F);
          r = (r << 3) | (r >> 2);  // Expand 5 bits to 8 bits
          g = ((color565 >> 5) & 0x3F);
          g = (g << 2) | (g >> 4);  // Expand 6 bits to 8 bits
          b = (color565 & 0x1F);
          b = (b << 3) | (b >> 2);  // Expand 5 bits to 8 bits
          a = 255;  // Fully opaque
        }
      }

      // Write RGBA to buffer
      lineBuffer[x * 4 + 0] = r;
      lineBuffer[x * 4 + 1] = g;
      lineBuffer[x * 4 + 2] = b;
      lineBuffer[x * 4 + 3] = a;
    }

    // Write this line to PNG
    rc = png->addLine(lineBuffer);
    if (rc != PNG_SUCCESS) {
      png->close();
      delete png;
      free(lineBuffer);
      free(pngBuffer);
      char msg[40];
      snprintf(msg, sizeof(msg), StatusMsg::ADDLINE_ERR_FMT, rc);
      setStatusMessage(msg);
      return false;
    }
  }

  // Close PNG and get final size
  int pngSize = png->close();
  delete png;
  free(lineBuffer);

  if (pngSize <= 0) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::PNG_ENCODE_FAIL);
    return false;
  }

  setStatusMessage(StatusMsg::WRITING_FILE);
  delay(50);

  Filesystem::createDirectory("/bitmap16dx/exports");

  // Generate filename with counter
  int exportNum = 0;
  char filename[40];
  do {
    snprintf(filename, sizeof(filename), "/bitmap16dx/exports/dx_%04d.png", exportNum);
    exportNum++;
  } while (Filesystem::exists(filename) && exportNum < 10000);

  if (exportNum >= 10000) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::TOO_MANY_EXPORTS);
    return false;
  }

  const bool wroteFile =
      Filesystem::writeFile(filename, pngBuffer, pngSize);
  free(pngBuffer);

  if (!wroteFile) {
    setStatusMessage(StatusMsg::WRITE_INCOMPLETE);
    return false;
  }

  setStatusMessage(StatusMsg::EXPORTED);
  return true;
}

#if ENABLE_SCREENSHOTS
/**
 * Take a screenshot of the full display (240×135 pixels)
 * Saves to /bitmap16dx/screenshots/screenshot_XXXX.png
 *
 * This captures the entire display buffer including UI elements,
 * not just the canvas area.
 *
 * @return true if successful, false if failed
 */
bool takeScreenshot() {
  // Ensure SD card is initialized
  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  // Check free heap before starting
  char heapMsg[40];
  snprintf(heapMsg, sizeof(heapMsg), StatusMsg::FREE_HEAP_FMT, (int)(ESP.getFreeHeap() / 1024));
  setStatusMessage(heapMsg);
  delay(500);

  setStatusMessage(StatusMsg::SCREENSHOT);
  delay(50);

  // Display dimensions
  const int displayWidth = 240;
  const int displayHeight = 135;

  // Allocate buffer for PNG output (16KB should be enough for 240×135)
  uint8_t* pngBuffer = (uint8_t*)malloc(PNG_BUFFER_SIZE);
  if (!pngBuffer) {
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  // Allocate line buffer for RGBA (4 bytes per pixel)
  uint8_t* lineBuffer = (uint8_t*)malloc(displayWidth * 4);
  if (!lineBuffer) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  // Allocate temporary buffer for reading RGB565 line from display
  uint16_t* displayLine = (uint16_t*)malloc(displayWidth * 2);
  if (!displayLine) {
    free(lineBuffer);
    free(pngBuffer);
    setStatusMessage(StatusMsg::OUT_OF_MEMORY);
    return false;
  }

  setStatusMessage(StatusMsg::ENCODING);
  delay(50);

  // Allocate PNG encoder on heap
  PNGENC* png = new PNGENC();
  if (!png) {
    free(displayLine);
    free(lineBuffer);
    free(pngBuffer);
    setStatusMessage(StatusMsg::PNG_ALLOC_FAIL);
    return false;
  }

  int rc = png->open(pngBuffer, PNG_BUFFER_SIZE);
  if (rc != PNG_SUCCESS) {
    delete png;
    free(displayLine);
    free(lineBuffer);
    free(pngBuffer);
    char msg[40];
    snprintf(msg, sizeof(msg), StatusMsg::PNG_OPEN_ERR_FMT, rc);
    setStatusMessage(msg);
    return false;
  }

  // Initialize PNG with RGBA format (compression level 3 for lower memory)
  rc = png->encodeBegin(displayWidth, displayHeight, PNG_PIXEL_TRUECOLOR_ALPHA, 32, NULL, 3);
  if (rc != PNG_SUCCESS) {
    png->close();
    delete png;
    free(displayLine);
    free(lineBuffer);
    free(pngBuffer);
    char msg[40];
    snprintf(msg, sizeof(msg), StatusMsg::PNG_INIT_ERR_FMT, rc);
    setStatusMessage(msg);
    return false;
  }

  // Read and encode each line from the display
  for (int y = 0; y < displayHeight; y++) {
    // Read one line of RGB565 pixels from the display
    M5Cardputer.Display.readRect(0, y, displayWidth, 1, displayLine);

    // Convert RGB565 to RGBA
    for (int x = 0; x < displayWidth; x++) {
      uint16_t color565 = displayLine[x];

      // M5Stack display returns RGB565 in little-endian format
      // Need to swap bytes: the data comes as [GGGBBBBB][RRRRRGGG]
      // Swap to get proper RGB565: [RRRRRGGG][GGGBBBBB]
      color565 = (color565 >> 8) | (color565 << 8);

      // Convert RGB565 to RGB888
      uint8_t r = ((color565 >> 11) & 0x1F);
      r = (r << 3) | (r >> 2);  // Expand 5 bits to 8 bits
      uint8_t g = ((color565 >> 5) & 0x3F);
      g = (g << 2) | (g >> 4);  // Expand 6 bits to 8 bits
      uint8_t b = (color565 & 0x1F);
      b = (b << 3) | (b >> 2);  // Expand 5 bits to 8 bits

      // Write RGBA to buffer (fully opaque)
      lineBuffer[x * 4 + 0] = r;
      lineBuffer[x * 4 + 1] = g;
      lineBuffer[x * 4 + 2] = b;
      lineBuffer[x * 4 + 3] = 255;  // Alpha
    }

    // Write this line to PNG
    rc = png->addLine(lineBuffer);
    if (rc != PNG_SUCCESS) {
      png->close();
      delete png;
      free(displayLine);
      free(lineBuffer);
      free(pngBuffer);
      char msg[40];
      snprintf(msg, sizeof(msg), StatusMsg::ADDLINE_ERR_FMT, rc);
      setStatusMessage(msg);
      return false;
    }
  }

  // Close PNG and get final size
  int pngSize = png->close();
  delete png;
  free(displayLine);
  free(lineBuffer);

  if (pngSize <= 0) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::PNG_ENCODE_FAIL);
    return false;
  }

  setStatusMessage(StatusMsg::WRITING);
  delay(50);

  Filesystem::createDirectory("/bitmap16dx/screenshots");

  // Generate filename with counter
  int screenshotNum = 0;
  char filename[48];
  do {
    snprintf(filename, sizeof(filename), "/bitmap16dx/screenshots/screenshot_%04d.png", screenshotNum);
    screenshotNum++;
  } while (Filesystem::exists(filename) && screenshotNum < 10000);

  if (screenshotNum >= 10000) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::TOO_MANY_SHOTS);
    return false;
  }

  const bool wroteFile =
      Filesystem::writeFile(filename, pngBuffer, pngSize);
  free(pngBuffer);

  if (!wroteFile) {
    setStatusMessage(StatusMsg::WRITE_FAIL);
    return false;
  }

  setStatusMessage(StatusMsg::SCREENSHOT_OK);
  return true;
}
#endif // ENABLE_SCREENSHOTS

/**
 * Draw the current status message if it's still active
 */
void drawStatusMessage() {
  // Check if we have an active message
  if (statusMessage[0] != '\0') {
    if (millis() - statusMessageTime < STATUS_DISPLAY_DURATION) {
      // Message is still active - check if it changed
      if (strcmp(statusMessage, lastDrawnMessage) != 0) {
        // New message - trigger redraw to replace old one
        statusMessageJustCleared = true;
        strncpy(lastDrawnMessage, statusMessage, sizeof(lastDrawnMessage) - 1);
        lastDrawnMessage[sizeof(lastDrawnMessage) - 1] = '\0';
      }
    } else {
      // Message expired - trigger clear
      if (lastDrawnMessage[0] != '\0') {
        statusMessageJustCleared = true;
        lastDrawnMessage[0] = '\0';
      }
      statusMessage[0] = '\0';
    }
  }
}

/**
 * Draw battery icon below fill icon
 * Only redraws when percentage changes
 * Checks battery level every 30 seconds to reduce flashing
 */
void drawBatteryIndicator() {
  unsigned long currentTime = millis();

  // Check if we need to force a redraw (when lastBatteryPercent is -1)
  bool forceRedraw = (lastBatteryPercent == -1);

  // Only check battery every 30 seconds (but always check on first call or forced)
  if (!forceRedraw && !batteryFirstCheck && currentTime - lastBatteryCheckTime < BATTERY_CHECK_INTERVAL) {
    return;
  }

  batteryFirstCheck = false;
  lastBatteryCheckTime = currentTime;

  // Get current battery percentage
  int batteryPercent = Power::getBatteryPercent();

  // The shared Canvas view draws the icon; this poll only updates indicator
  // state and the cached percentage.
  if (batteryPercent != lastBatteryPercent || forceRedraw) {
    // Low battery LED indicator (latches red at <= 10%)
    if (!lowBattery && batteryPercent <= 10) {
      lowBattery = true;
      Indicator::setColor(255, 0, 0);
    }

    lastBatteryPercent = batteryPercent;
  }
}

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void drawSharedCanvasView();
void drawHelpView();
void drawMemoryView(bool fullRedraw = true);
void updatePaletteFilter();
void loadGallerySketch(int index);  // Load and display sketch in gallery preview mode

#if ENABLE_LED_MATRIX
// LED matrix support functions
void updateLEDMatrix(bool showCursor = true);
void updateLEDMatrixFromSketch(Sketch& sketch);
void toggleLEDMatrix();
#endif

void drawSharedCanvasView() {
  if (!Display::isReady() && !Display::init()) {
    return;
  }
  const bitmap16::CanvasView::State state = {
      editorState.canvas,
      static_cast<uint8_t>(editorState.gridSize),
      documentState.sketch.paletteColors,
      documentState.sketch.paletteSize,
      static_cast<uint8_t>(editorState.cursorX),
      static_cast<uint8_t>(editorState.cursorY),
      editorState.selectedColor,
      editorState.rulersVisible,
      editorState.moveModeActive,
      statusMessage,
      Power::getBatteryPercent(),
      editorState.drawPressed,
      editorState.erasePressed,
      editorState.fillPressed,
  };
  const bitmap16::CanvasView::Theme theme = {
      currentTheme->background,
      currentTheme->cellDark,
      currentTheme->cellLight,
      currentTheme->shadow,
      currentTheme->text,
      currentTheme->textSecondary,
      currentTheme->centerLine,
      currentTheme->iconDark,
      currentTheme->iconLight,
      currentTheme == &THEME_DARK,
  };
  const bitmap16::CanvasView::Assets assets = {
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
  bitmap16::CanvasView::render(Display::canvas(), state, theme, &assets);
  Display::endFrame();
}

#if ENABLE_BLUETOOTH
// Bluetooth keyboard support functions
void btInit();
void btDeinit();
void btStartScan();
bool btConnect();
bool btReconnect();
void btDisconnect();
void btProcessHIDReport(uint8_t* data, size_t len);
char btHidToChar(uint8_t keycode, bool shift);
void btQueuePush(char c);
bool btQueuePop(char& c);
void btClearInputState();
void btShowNotify(const char* msg);
void btUpdateNotify();
#endif

// ============================================================================
// CANVAS OPERATIONS
// ============================================================================

/**
 * Save current canvas state to undo buffer
 * Note: This is for regular drawing undo, not sketch deletion operations
 */
void saveUndo() {
  for (int y = 0; y < editorState.gridSize; y++) {
    for (int x = 0; x < editorState.gridSize; x++) {
      editorState.undoCanvas[y][x] = editorState.canvas[y][x];
    }
  }
  // Clear palette undo info (this is just a regular drawing undo)
  editorState.undoPaletteSize = 0;
  editorState.undoGridSize = 0;
  editorState.undoAvailable = true;
  editorState.redoAvailable = false;
}

/**
 * Restore canvas from undo buffer
 */
void restoreUndo() {
  if (!editorState.undoAvailable) {
    setStatusMessage(StatusMsg::NO_UNDO);
    return;
  }

  // Preserve the current document so Fn+Z can redo this undo.
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      editorState.redoCanvas[y][x] = editorState.canvas[y][x];
    }
  }
  editorState.redoGridSize = editorState.gridSize;
  editorState.redoPaletteSize = documentState.sketch.paletteSize;
  for (int i = 0; i < 16; i++) {
    editorState.redoPaletteColors[i] = documentState.sketch.paletteColors[i];
  }
  editorState.redoAvailable = true;

  // If we have saved grid size info (from sketch deletion), restore it
  if (editorState.undoGridSize > 0) {
    editorState.gridSize = editorState.undoGridSize;
    editorState.cellSize = (editorState.gridSize == 8) ? 16 : 8;

    // Keep cursor in bounds
    if (editorState.cursorX >= editorState.gridSize) editorState.cursorX = editorState.gridSize - 1;
    if (editorState.cursorY >= editorState.gridSize) editorState.cursorY = editorState.gridSize - 1;
  }

  // Restore all pixels (always restore full 16x16 to handle grid size changes)
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      editorState.canvas[y][x] = editorState.undoCanvas[y][x];
    }
  }

  // If we have palette info saved (from sketch deletion), restore it to the active sketch
  if (editorState.undoPaletteSize > 0) {
    documentState.sketch.paletteSize = editorState.undoPaletteSize;
    documentState.sketch.gridSize = editorState.undoGridSize;
    for (int i = 0; i < 16; i++) {
      documentState.sketch.paletteColors[i] = editorState.undoPaletteColors[i];
    }
  }

  editorState.undoAvailable = false;

  // Update LED matrix with restored canvas
  LED_CANVAS_UPDATED();

  setStatusMessage(StatusMsg::UNDO);
}

void restoreRedo() {
  if (!editorState.redoAvailable) {
    setStatusMessage("No redo");
    return;
  }

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      editorState.undoCanvas[y][x] = editorState.canvas[y][x];
      editorState.canvas[y][x] = editorState.redoCanvas[y][x];
    }
  }
  editorState.undoGridSize = editorState.gridSize;
  editorState.undoPaletteSize = documentState.sketch.paletteSize;
  for (int i = 0; i < 16; i++) {
    editorState.undoPaletteColors[i] = documentState.sketch.paletteColors[i];
    documentState.sketch.paletteColors[i] = editorState.redoPaletteColors[i];
  }
  editorState.undoAvailable = true;

  editorState.gridSize = editorState.redoGridSize;
  editorState.cellSize = editorState.gridSize == 8 ? 16 : 8;
  documentState.sketch.gridSize = editorState.redoGridSize;
  documentState.sketch.paletteSize = editorState.redoPaletteSize;
  if (editorState.cursorX >= editorState.gridSize) {
    editorState.cursorX = editorState.gridSize - 1;
  }
  if (editorState.cursorY >= editorState.gridSize) {
    editorState.cursorY = editorState.gridSize - 1;
  }
  editorState.redoAvailable = false;
  LED_CANVAS_UPDATED();
  setStatusMessage("Redo");
}

/**
 * Clear the entire canvas
 * This saves the current state to undo before clearing
 */
void clearCanvas() {
  saveUndo();

  for (int y = 0; y < editorState.gridSize; y++) {
    for (int x = 0; x < editorState.gridSize; x++) {
      editorState.canvas[y][x] = 0;
    }
  }

  setStatusMessage(StatusMsg::CLEAR);
}

// Shift the entire canvas by (dx, dy) with wrapping
void shiftCanvas(int dx, int dy) {
  uint8_t temp[16][16];
  int size = editorState.gridSize;
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      int srcX = (x - dx + size) % size;
      int srcY = (y - dy + size) % size;
      temp[y][x] = editorState.canvas[srcY][srcX];
    }
  }
  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      editorState.canvas[y][x] = temp[y][x];
    }
  }
}

/**
 * Flood fill - fills all connected pixels of the same color with the selected color
 *
 * This is like the paint bucket tool in image editors. Starting from the cursor position,
 * it fills all adjacent pixels that match the original color with the currently selected color.
 *
 * Uses an iterative approach with a simple array-based stack to avoid recursion issues.
 * Only considers 4-way connectivity (up, down, left, right) - not diagonal.
 *
 * @param startX Starting X position (cursor position)
 * @param startY Starting Y position (cursor position)
 * @param fillColor Color to fill with (currently selected color)
 */
void floodFill(int startX, int startY, uint8_t fillColor) {
  // Get the original color at the starting position
  uint8_t originalColor = editorState.canvas[startY][startX];

  // If the original color is the same as fill color, nothing to do
  if (originalColor == fillColor) {
    return;
  }

  // Visited array to track which pixels we've already added to the stack
  // This prevents duplicates and ensures we process each pixel exactly once
  bool visited[16][16] = {false};

  // Simple stack structure for positions to check
  // Maximum possible stack size is the entire grid (16×16 = 256 positions)
  struct Point {
    int x;
    int y;
  };
  Point stack[256];
  int stackSize = 0;

  // Add starting position to stack and mark as visited
  stack[stackSize++] = {startX, startY};
  visited[startY][startX] = true;

  // Process stack until empty
  while (stackSize > 0) {
    // Pop position from stack
    Point p = stack[--stackSize];

    // Skip if out of bounds (shouldn't happen, but safety check)
    if (p.x < 0 || p.x >= editorState.gridSize || p.y < 0 || p.y >= editorState.gridSize) {
      continue;
    }

    // Skip if this pixel isn't the original color
    if (editorState.canvas[p.y][p.x] != originalColor) {
      continue;
    }

    // Fill this pixel
    editorState.canvas[p.y][p.x] = fillColor;

    // Add adjacent pixels to stack (4-way connectivity: up, down, left, right)
    // Only add if not visited and within bounds
    // Up
    if (p.y > 0 && !visited[p.y - 1][p.x]) {
      stack[stackSize++] = {p.x, p.y - 1};
      visited[p.y - 1][p.x] = true;
    }
    // Down
    if (p.y < editorState.gridSize - 1 && !visited[p.y + 1][p.x]) {
      stack[stackSize++] = {p.x, p.y + 1};
      visited[p.y + 1][p.x] = true;
    }
    // Left
    if (p.x > 0 && !visited[p.y][p.x - 1]) {
      stack[stackSize++] = {p.x - 1, p.y};
      visited[p.y][p.x - 1] = true;
    }
    // Right
    if (p.x < editorState.gridSize - 1 && !visited[p.y][p.x + 1]) {
      stack[stackSize++] = {p.x + 1, p.y};
      visited[p.y][p.x + 1] = true;
    }
  }
}

/**
 * Toggle between 8×8 and 16×16 grid modes
 *
 * When switching from 16×16 to 8×8, the top-left 8×8 portion is preserved.
 * When switching from 8×8 to 16×16, the 8×8 art appears in the top-left.
 */
void toggleGridSize() {
  // Toggle between 8 and 16
  if (editorState.gridSize == 8) {
    editorState.gridSize = 16;
    editorState.cellSize = 8;  // Smaller cells for more pixels
    setStatusMessage(StatusMsg::GRID_16X16);
  } else {
    editorState.gridSize = 8;
    editorState.cellSize = 16;  // Larger cells for fewer pixels
    setStatusMessage(StatusMsg::GRID_8X8);
  }

  // Keep cursor in bounds
  if (editorState.cursorX >= editorState.gridSize) editorState.cursorX = editorState.gridSize - 1;
  if (editorState.cursorY >= editorState.gridSize) editorState.cursorY = editorState.gridSize - 1;

  // Update LED matrix (turn off in 16×16 mode, turn on in 8×8 mode)
  LED_CANVAS_UPDATED();
}

/**
 * Open a sketch from SD card by filename
 */
void openSketch(String filename) {
  if (!loadSketchFromSD(filename)) {
    setStatusMessage(StatusMsg::FAILED_TO_LOAD);
    return;
  }

  // Validate palette
  if (documentState.sketch.paletteSize == 0 || documentState.sketch.paletteSize > 16) {
    documentState.sketch.paletteSize = 16;
  }

  // Copy to canvas
  editorState.gridSize = documentState.sketch.gridSize;
  editorState.cellSize = (editorState.gridSize == 8) ? 16 : 8;

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      editorState.canvas[y][x] = documentState.sketch.pixels[y][x];
    }
  }

  if (editorState.cursorX >= editorState.gridSize) editorState.cursorX = editorState.gridSize - 1;
  if (editorState.cursorY >= editorState.gridSize) editorState.cursorY = editorState.gridSize - 1;

  editorState.selectedColor = 1;

  // Update LED matrix with newly loaded canvas
  LED_CANVAS_UPDATED();

  setStatusMessage(StatusMsg::LOADED);
}

/**
 * Create a new blank sketch
 */
void createNewSketch() {
  initializeActiveSketch();

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      editorState.canvas[y][x] = 0;
    }
  }

  // Use default grid size from settings instead of hardcoded 16
  editorState.gridSize = defaultGridSize;
  editorState.cellSize = (editorState.gridSize == 16) ? 8 : 16;
  documentState.sketch.gridSize = editorState.gridSize;
  editorState.cursorX = 0;
  editorState.cursorY = 0;
  editorState.selectedColor = 1;

  // setStatusMessage("New sketch");  // Removed - no message on boot
}

/**
 * Enter Memory View mode
 */
bitmap16::MemoryView::Catalog currentMemoryCatalog() {
  static std::vector<bitmap16::MemoryView::Entry> entries;
  entries.resize(sketchList.size());
  for (int index = 0; index < sketchList.size(); ++index) {
    SketchInfo& info = sketchList[index];
    if (!info.dataLoaded) {
      const String path = "/bitmap16dx/sketches/" + info.filename;
      info.dataLoaded = readSketchFile(path.c_str(), info.sketchData);
    }
    entries[index] = {
        info.dataLoaded ? info.sketchData.pixels : nullptr,
        info.dataLoaded ? info.sketchData.gridSize : static_cast<uint8_t>(8),
        info.dataLoaded ? info.sketchData.paletteColors : nullptr,
        info.dataLoaded ? info.sketchData.paletteSize : static_cast<uint8_t>(16),
        info.filename == documentState.filename && !documentState.isNew,
    };
  }
  return {
      entries.empty() ? nullptr : entries.data(),
      static_cast<int>(entries.size()),
  };
}

void enterMemoryView() {
  loadSketchListFromSD();  // Load sketch list (sketch data will be cached on first draw)
  app.setView(bitmap16::ViewId::Memory);
  bitmap16::MemoryView::clamp(
      viewState.memory.navigation, sketchList.size());
  viewState.memory.lastAnimationTime = millis();
  viewState.memory.navigation.cursorAnimationPhase = 0.0f;
  viewState.memory.ownsCanvas = !Display::isReady();
  viewState.memory.canvasAvailable = Display::init();
  drawMemoryView(true);
}

/**
 * Exit Memory View and return to canvas
 */
void exitMemoryView() {
  app.setView(bitmap16::ViewId::Canvas);
  if (viewState.memory.ownsCanvas) {
    Display::shutdown();
  }
  viewState.memory.canvasAvailable = false;
  viewState.memory.ownsCanvas = false;
  sketchList.clear();
  sketchList.shrink_to_fit();

  drawSharedCanvasView();

#if ENABLE_LED_MATRIX
  // Restore active canvas on LED matrix
  updateLEDMatrix();
#endif

}

/**
 * Enter Charging Mode - DVD-style bouncing battery screensaver
 */
const uint8_t* chargingBatteryIcon(int batteryPercent) {
  if (batteryPercent < 10) return ICON_BATTERY_0;
  if (batteryPercent < 50) return ICON_BATTERY_10;
  if (batteryPercent < 90) return ICON_BATTERY_50;
  return ICON_BATTERY_90;
}

void drawChargingFrame() {
  if (!viewState.charging.canvasAvailable) {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 60);
    M5Cardputer.Display.println("CHARGE BUFFER FAILED");
    return;
  }

  const bitmap16::ChargingView::Theme theme = {
      TFT_BLACK,
      THEME_DARK.iconDark,
      THEME_DARK.iconLight,
      THEME_DARK.text,
  };
  bitmap16::ChargingView::SketchImage sketch;
  const bitmap16::ChargingView::SketchImage* sketchPointer = nullptr;
  if (viewState.charging.sketchLoaded) {
    sketch = {
        viewState.charging.sketch.pixels,
        viewState.charging.sketch.gridSize,
        viewState.charging.sketch.paletteColors,
        viewState.charging.sketch.paletteSize,
    };
    sketchPointer = &sketch;
  }
  bitmap16::ChargingView::render(
      Display::canvas(),
      viewState.charging.animation,
      theme,
      sketchPointer);
  Display::endFrame();
}

void enterChargingMode() {
  app.setView(bitmap16::ViewId::Charging);
  viewState.charging.lastFrameTime = millis();
  viewState.charging.batteryPercent = Power::getBatteryPercent();
  viewState.charging.lastBatteryCheck = millis();

  // Load a random sketch for the shared renderer.
  viewState.charging.sketchLoaded = false;
  loadSketchListFromSD();

  if (sketchList.size() > 0) {
    // Pick a random sketch
    int randIndex = millis() % sketchList.size();
    SketchInfo& info = sketchList[randIndex];

    // Load sketch data from storage
    String fullPath = "/bitmap16dx/sketches/" + info.filename;
    viewState.charging.sketchLoaded =
        readSketchFile(fullPath.c_str(), viewState.charging.sketch);
  }

  viewState.charging.ownsCanvas = !Display::isReady();
  viewState.charging.canvasAvailable = Display::init();
  const uint8_t* const icons[4] = {
      ICON_DRAW,
      ICON_ERASE,
      ICON_FILL,
      chargingBatteryIcon(viewState.charging.batteryPercent),
  };
  bitmap16::ChargingView::initialize(
      viewState.charging.animation,
      M5Cardputer.Display.width(),
      M5Cardputer.Display.height(),
      millis(),
      icons,
      viewState.charging.batteryPercent,
      viewState.charging.sketchLoaded);

  // Dim display
  Display::setBrightness(20);

  drawChargingFrame();
}

/**
 * Exit Charging Mode - restore canvas view
 */
void exitChargingMode() {
  app.setView(bitmap16::ViewId::Canvas);

  if (viewState.charging.ownsCanvas) {
    Display::shutdown();
  }
  viewState.charging.canvasAvailable = false;
  viewState.charging.ownsCanvas = false;
  viewState.charging.sketchLoaded = false;

  // Restore brightness
  Display::setBrightness(displayBrightness);

  drawSharedCanvasView();
  drawBatteryIndicator();
}

/**
 * Enter Help Screen mode
 */
void enterHelpView() {
  app.setView(bitmap16::ViewId::Help);
  viewState.help.navigation = {};
  viewState.help.metrics = {};
  viewState.help.metrics.minimumFreeHeap = UINT32_MAX;
  viewState.help.metrics.heapBeforeAllocation = ESP.getFreeHeap();
  const uint32_t allocationStart = micros();
  viewState.help.ownsCanvas = !Display::isReady();
  viewState.help.softwareCanvas = Display::init();
  viewState.help.metrics.allocationMicros = micros() - allocationStart;
  viewState.help.metrics.heapAfterAllocation = ESP.getFreeHeap();
  viewState.help.metrics.minimumFreeHeap =
      viewState.help.metrics.heapAfterAllocation;

  viewState.help.canvasAvailable = viewState.help.softwareCanvas;

#if ENABLE_CANVAS_PROOF_TELEMETRY
  Serial.printf(
      "[canvas-proof] allocation=%luus heap_before=%lu heap_after=%lu "
      "heap_delta=%ld backend=%s success=%d\n",
      static_cast<unsigned long>(viewState.help.metrics.allocationMicros),
      static_cast<unsigned long>(
          viewState.help.metrics.heapBeforeAllocation),
      static_cast<unsigned long>(
          viewState.help.metrics.heapAfterAllocation),
      static_cast<long>(viewState.help.metrics.heapAfterAllocation) -
          static_cast<long>(viewState.help.metrics.heapBeforeAllocation),
      viewState.help.softwareCanvas ? "software" : "unavailable",
      viewState.help.canvasAvailable);
#endif

  if (!viewState.help.canvasAvailable) {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(8, 58);
    M5Cardputer.Display.print("HELP BUFFER FAILED");
    return;
  }

  // Draw help screen
  drawHelpView();
  if (viewState.help.softwareCanvas) {
    // A second initial presentation prevents a partial first LCD transfer from
    // leaving pixels from the previous static view until the first input.
    drawHelpView();
  }
}

/**
 * Exit Help Screen and return to previous view
 */
void exitHelpView() {
  const bitmap16::ViewId returnView =
      app.previousView() == bitmap16::ViewId::Memory
          ? bitmap16::ViewId::Memory
          : bitmap16::ViewId::Canvas;
  app.setView(returnView);

#if ENABLE_CANVAS_PROOF_TELEMETRY
  Serial.printf(
      "[canvas-proof] summary frames=%lu max_render=%luus "
      "max_blit=%luus min_heap=%lu\n",
      static_cast<unsigned long>(viewState.help.metrics.frameCount),
      static_cast<unsigned long>(viewState.help.metrics.maxRenderMicros),
      static_cast<unsigned long>(viewState.help.metrics.maxBlitMicros),
      static_cast<unsigned long>(viewState.help.metrics.minimumFreeHeap));
#endif

  if (viewState.help.softwareCanvas && viewState.help.ownsCanvas) {
    Display::shutdown();
  }
  viewState.help.canvasAvailable = false;
  viewState.help.softwareCanvas = false;
  viewState.help.ownsCanvas = false;

  // Return to the view we came from
  if (returnView == bitmap16::ViewId::Memory) {
    // Return to memory view
    drawMemoryView(true);
  } else {
    drawSharedCanvasView();
  }
}

/**
 * Load and display a sketch from the gallery in fullscreen preview
 * Uses the cached sketch list populated by the Memory view
 */
bitmap16::PreviewView::Theme previewTheme() {
  return {
      VIEW_BG_BLACK,
      VIEW_BG_WHITE,
      VIEW_BG_GRAY,
      VIEW_BG_DARK,
  };
}

void drawPreviewImage(
    const uint8_t pixels[][16],
    uint8_t gridSize,
    const uint16_t* paletteColors,
    uint8_t paletteSize) {
  if (!viewState.preview.canvasAvailable) {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 50);
    M5Cardputer.Display.println("WARNING: Low memory!");
    M5Cardputer.Display.setCursor(10, 65);
    M5Cardputer.Display.println("Cannot show preview.");
    return;
  }

  const bitmap16::PreviewView::Image image = {
      pixels,
      gridSize,
      paletteColors,
      paletteSize,
  };
  bitmap16::PreviewView::render(
      Display::canvas(),
      viewState.preview.display,
      image,
      previewTheme());
  Display::endFrame();
}

void drawCanvasPreview() {
  drawPreviewImage(
      editorState.canvas,
      editorState.gridSize,
      documentState.sketch.paletteColors,
      documentState.sketch.paletteSize);
}

void loadGallerySketch(int index) {
  if (index < 0 || index >= sketchList.size()) {
    return;
  }

  SketchInfo& info = sketchList[index];

  // Load data from SD if not already cached
  if (!info.dataLoaded) {
    String fullPath = "/bitmap16dx/sketches/" + info.filename;
    if (!readSketchFile(fullPath.c_str(), info.sketchData)) {
      setStatusMessage(StatusMsg::FILE_OPEN_FAIL);
      return;
    }
    info.dataLoaded = true;  // Mark as cached
  }

  // Render cached sketch through the shared preview view.
  Sketch& sketch = info.sketchData;
  drawPreviewImage(
      sketch.pixels,
      sketch.gridSize,
      sketch.paletteColors,
      sketch.paletteSize);

#if ENABLE_LED_MATRIX
  // Update LED matrix to mirror the sketch (8×8 only)
  updateLEDMatrixFromSketch(sketch);
#endif
}

/**
 * Enter View Mode - display canvas at 128×128 with selected background
 * Context-aware: detects if coming from Memory View for gallery mode
 */
void enterPreviewView() {
  const bool fromMemoryView =
      app.currentView() == bitmap16::ViewId::Memory;
  app.setView(bitmap16::ViewId::Preview);
  viewState.preview.ownsCanvas = !Display::isReady();
  viewState.preview.canvasAvailable = Display::init();

  // Check if we're coming from Memory View (gallery mode)
  if (fromMemoryView) {
    viewState.preview.galleryMode = true;
    viewState.preview.autoAdvance = false;  // Start paused

    // Start at selected sketch (viewState.memory.cursor - 1 because cursor 0 is "+")
    if (viewState.memory.navigation.cursor > 0 &&
        viewState.memory.navigation.cursor - 1 < sketchList.size()) {
      viewState.preview.galleryIndex =
          viewState.memory.navigation.cursor - 1;
    } else {
      viewState.preview.galleryIndex = 0;  // Fallback to first sketch
    }

    viewState.preview.lastAdvanceTime = millis();

    // Load and display sketch from gallery
    loadGallerySketch(viewState.preview.galleryIndex);
    return;  // Exit early - loadGallerySketch handles rendering
  }

  // Canvas preview mode (not from Memory View)
  viewState.preview.galleryMode = false;
  drawCanvasPreview();

#if ENABLE_LED_MATRIX
  // Update LED matrix to mirror the live canvas (8×8 only, no cursor)
  updateLEDMatrix(false);
#endif
}

/**
 * Exit View Mode and return to canvas or Memory View
 * Context-aware: returns to Memory View if in gallery mode
 */
void exitPreviewView() {
  if (viewState.preview.ownsCanvas) {
    Display::shutdown();
  }
  viewState.preview.canvasAvailable = false;
  viewState.preview.ownsCanvas = false;

  if (viewState.preview.galleryMode) {
    // Return to Memory View at current gallery position
    viewState.memory.navigation.cursor =
        viewState.preview.galleryIndex + 1;  // +1 for "+" button offset
    viewState.preview.galleryMode = false;
    viewState.preview.autoAdvance = false;
    app.setView(bitmap16::ViewId::Memory);

    // Redraw Memory View
    M5Cardputer.Display.fillScreen(currentTheme->background);
    drawMemoryView(true);

#if ENABLE_LED_MATRIX
    // Keep displaying the currently viewed sketch on LED matrix
    if (viewState.preview.galleryIndex >= 0 && viewState.preview.galleryIndex < sketchList.size()) {
      SketchInfo& info = sketchList[viewState.preview.galleryIndex];
      if (info.dataLoaded) {
        updateLEDMatrixFromSketch(info.sketchData);
      } else {
        // If data not loaded, clear LED matrix
        LEDMatrix::clear();
        LEDMatrix::show();
      }
    } else {
      // Invalid index, clear LED matrix
      LEDMatrix::clear();
      LEDMatrix::show();
    }
#endif

    return;  // Exit early
  }

  // Return to canvas view (existing behavior)
  app.setView(bitmap16::ViewId::Canvas);
  drawSharedCanvasView();

#if ENABLE_LED_MATRIX
  // Restore canvas display on LED matrix
  updateLEDMatrix();
#endif
}

/**
 * Enter Palette Menu - horizontally scrolling palette selector
 */
bitmap16::PaletteView::Catalog currentPaletteCatalog() {
  static bitmap16::PaletteView::Entry entries[32];
  const int count = min(32, static_cast<int>(totalPaletteCount));
  for (int index = 0; index < count; ++index) {
    entries[index] = {
        allPalettes[index],
        allPaletteNames[index],
        allPaletteSizes[index],
        paletteIsUserLoaded[index],
    };
  }
  return {entries, count};
}

int activePaletteCatalogIndex() {
  for (int palette = 0; palette < totalPaletteCount; ++palette) {
    bool matches = true;
    for (int color = 0; color < 16; ++color) {
      if (documentState.sketch.paletteColors[color] !=
          pgm_read_word(&allPalettes[palette][color])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return palette;
    }
  }
  return -1;
}

void enterPaletteView() {
  app.setView(bitmap16::ViewId::Palette);
  viewState.palette.ownsCanvas = !Display::isReady();
  viewState.palette.canvasAvailable = Display::init();
  const bitmap16::PaletteView::Catalog catalog = currentPaletteCatalog();
  bitmap16::PaletteView::reset(viewState.palette.navigation, catalog);
  bitmap16::PaletteView::selectCatalogIndex(
      viewState.palette.navigation, activePaletteCatalogIndex());
}

/**
 * Exit Palette Menu and return to canvas
 */
void exitPaletteView() {
  app.setView(bitmap16::ViewId::Canvas);

  if (viewState.palette.ownsCanvas) {
    Display::shutdown();
  }
  viewState.palette.canvasAvailable = false;
  viewState.palette.ownsCanvas = false;

  drawSharedCanvasView();
}

// ============================================================================
// SETTINGS VIEW
// ============================================================================

bitmap16::Settings currentSettingsValues() {
  bitmap16::Settings settings;
  settings.theme = currentTheme == &THEME_DARK
      ? bitmap16::ThemeId::Dark
      : bitmap16::ThemeId::Light;
  settings.defaultGridSize = defaultGridSize;
  settings.matrixUnits = rgbMatrixUnits;
  settings.matrixRotation = matrixRotation;
  settings.exportFormat = exportRGB565
      ? bitmap16::ExportFormat::Rgb565
      : bitmap16::ExportFormat::Rgb888;
  settings.shakeUndoEnabled = shakeUndoEnabled;
  settings.displayBrightness = displayBrightness;
#if ENABLE_LED_MATRIX
  settings.matrixBrightness = ledBrightness;
#endif
  return settings;
}

#if ENABLE_BLUETOOTH
const char* bluetoothSettingsValue() {
  static char value[16];
  if (btConnected) return "Paired";
  if (btScanning) {
    snprintf(value, sizeof(value), "Scan %d", btScanCountdown);
    return value;
  }
  if (btEnabled && btHasBondedDevice) return "Reconnect";
  if (btEnabled) return "Scan";
  return "OFF";
}
#endif

/**
 * Enter Settings Menu - vertical list of persistent preferences
 */
void enterSettingsView() {
  app.setView(bitmap16::ViewId::Settings);
  viewState.settings.navigation = {};
  viewState.settings.ownsCanvas = !Display::isReady();
  viewState.settings.canvasAvailable = Display::init();
}

/**
 * Exit Settings Menu and return to canvas
 */
void exitSettingsView() {
  app.setView(bitmap16::ViewId::Canvas);

  if (viewState.settings.ownsCanvas) {
    Display::shutdown();
  }
  viewState.settings.canvasAvailable = false;
  viewState.settings.ownsCanvas = false;

  drawSharedCanvasView();
  drawBatteryIndicator();
}


/**
 * Draw Settings Menu UI to canvas
 */
void drawSettingsView() {
  if (!viewState.settings.canvasAvailable) {
    M5Cardputer.Display.fillScreen(currentTheme->background);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 50);
    M5Cardputer.Display.println("WARNING: Low memory!");
    M5Cardputer.Display.setCursor(10, 65);
    M5Cardputer.Display.println("Cannot show settings.");
    M5Cardputer.Display.setCursor(10, 85);
    M5Cardputer.Display.setTextColor(currentTheme->text);
    M5Cardputer.Display.println("Press ESC (`) to exit");
    return;
  }

  bitmap16::SettingsView::Theme theme;
  theme.background = currentTheme->background;
  theme.text = currentTheme->text;
  theme.textSecondary = currentTheme->textSecondary;
  const bool showStatus =
      statusMessage[0] != '\0' &&
      millis() - statusMessageTime < STATUS_DISPLAY_DURATION;
  bitmap16::SettingsView::render(
      Display::canvas(),
      viewState.settings.navigation,
      currentSettingsValues(),
      theme,
      ENABLE_BLUETOOTH != 0,
      true,
      true,
#if ENABLE_BLUETOOTH
      bluetoothSettingsValue(),
#else
      nullptr,
#endif
      showStatus ? statusMessage : nullptr);
  Display::endFrame();
}

/**
 * Handle Settings Menu input and navigation
 */
void handleSettingsView(const bitmap16::InputFrame& input) {
  // Track if we need to redraw
  static bool settingsViewNeedsRedraw = true;
  static int lastSettingsViewCursor = -1;

  // Redraw if cursor changed or first time
  if (settingsViewNeedsRedraw || lastSettingsViewCursor != viewState.settings.navigation.cursor) {
    drawSettingsView();
    settingsViewNeedsRedraw = false;
    lastSettingsViewCursor = viewState.settings.navigation.cursor;
  }

  // Check for BT enter (edge-triggered)
#if ENABLE_BLUETOOTH
  static bool btPrevEnterSettings = false;
  bool btEnterPressed = btEnter && !btPrevEnterSettings;
  btPrevEnterSettings = btEnter;
#else
  bool btEnterPressed = false;
#endif

  if (input.keyboardHeld || btEnterPressed) {
    const bool activateSelected =
        input.enterPressed || btEnterPressed ||
        input.event == bitmap16::InputEvent::Left ||
        input.event == bitmap16::InputEvent::Right ||
        input.event == bitmap16::InputEvent::Space;

    if (activateSelected) {

      switch(viewState.settings.navigation.cursor) {
        case 0:  // Theme
          // Toggle theme
          if (currentTheme == &THEME_LIGHT) {
            currentTheme = &THEME_DARK;
            setStatusMessage("Dark Mode");
          } else {
            currentTheme = &THEME_LIGHT;
            setStatusMessage("Light Mode");
          }

          // Save preference
          PreferenceStore::writeBool(
              "darkMode",
              currentTheme == &THEME_DARK);

          break;

        case 1:  // Default Grid Size
          // Toggle between 8×8 and 16×16
          defaultGridSize = (defaultGridSize == 8) ? 16 : 8;

          // Save preference
          PreferenceStore::writeUInt8("defaultGrid", defaultGridSize);

          setStatusMessage(defaultGridSize == 8 ? "Default: 8x8" : "Default: 16x16");
          break;

        case 2:  // RGB Matrix Units
          // Toggle between 1 and 4 units
          rgbMatrixUnits = (rgbMatrixUnits == 1) ? 4 : 1;

          // Save preference
          PreferenceStore::writeUInt8("puzzleUnits", rgbMatrixUnits);

#if ENABLE_LED_MATRIX
          LEDMatrix::setConfiguration(rgbMatrixUnits, matrixRotation);
          // Clear all LEDs first (in case going from 4 to 1 unit)
          LEDMatrix::clear();
          LEDMatrix::show();

          // Update LED matrix immediately with current canvas
          if (LEDMatrix::isEnabled()) {
            updateLEDMatrix(false);  // Show editorState.canvas without cursor while in settings
          }
#endif

          setStatusMessage(rgbMatrixUnits == 1 ? "1 Unit" : "4 Units");
          break;

        case 3:  // Matrix Rotation
          matrixRotation = (matrixRotation + 1) % 4;

          // Save preference
          PreferenceStore::writeUInt8("matrixRot", matrixRotation);

#if ENABLE_LED_MATRIX
          LEDMatrix::setConfiguration(rgbMatrixUnits, matrixRotation);
          if (LEDMatrix::isEnabled()) {
            updateLEDMatrix(false);
          }
#endif

          {
            char rotMsg[16];
            snprintf(rotMsg, sizeof(rotMsg), "Rotation: %d", matrixRotation * 90);
            setStatusMessage(rotMsg);
          }
          break;

        case 4:  // Export Format
          // Toggle between RGB888 and RGB565
          exportRGB565 = !exportRGB565;

          // Save preference
          PreferenceStore::writeBool("exportRGB565", exportRGB565);

          setStatusMessage(exportRGB565 ? "Export: RGB565" : "Export: RGB888");
          break;

        case 5:  // Shake Undo
          // Toggle shake-to-undo
          shakeUndoEnabled = !shakeUndoEnabled;

          // Save preference
          PreferenceStore::writeBool("shakeUndo", shakeUndoEnabled);

          setStatusMessage(shakeUndoEnabled ? "Shake: ON" : "Shake: OFF");
          break;

#if ENABLE_BLUETOOTH
        case 6:  // Bluetooth
          if (btConnected) {
            // Disconnect if connected (Fn+Enter forgets pairing too)
            btDisconnect();
            btDeinit();  // Free BLE memory
            if (input.fnHeld) {
              // Fn held - forget bonded device
              btHasBondedDevice = false;
              PreferenceStore::writeBool("btHasBonded", false);
              setStatusMessage("BT Forgotten");
            } else {
              setStatusMessage("BT Disconnected");
            }
          } else if (btScanning) {
            // Already scanning, do nothing
          } else if (btEnabled && btHasBondedDevice && !input.fnHeld) {
            // Try to reconnect to bonded device (no scan needed)
            // Fn bypasses this to force new scan
            if (btReconnect()) {
              setStatusMessage("BT Connected!");
            } else {
              setStatusMessage("Reconnect failed");
            }
          } else if (btEnabled) {
            // Start scanning for keyboard (no bonded device, or Fn held to force scan)
            if (input.fnHeld && btHasBondedDevice) {
              // Forget old pairing first
              btHasBondedDevice = false;
              PreferenceStore::writeBool("btHasBonded", false);
            }
            btStartScan();
            if (btAdvDevice) {
              btConnect();
              if (btConnected) {
                // Save bonded device
                PreferenceStore::writeBytes("btBonded", btBondedAddr, 6);
                PreferenceStore::writeBool("btHasBonded", true);
                setStatusMessage("BT Connected!");
              } else {
                btDeinit();  // Free BLE memory on connect failure
                setStatusMessage("Connect failed");
              }
            } else {
              btDeinit();  // Free BLE memory - no keyboard found
              char msg[32];
              snprintf(msg, sizeof(msg), "No kbd (%d devices)", btLastScanCount);
              setStatusMessage(msg);
            }
          } else {
            // Enable Bluetooth (stack will init on scan/reconnect)
            btEnabled = true;
            PreferenceStore::writeBool("btEnabled", btEnabled);
            setStatusMessage(btHasBondedDevice ? "BT On - Reconnect" : "BT On - Scan");
          }
          break;
#endif
      }

      // Force redraw (especially important for theme changes)
      settingsViewNeedsRedraw = true;

    }

    if (input.event == bitmap16::InputEvent::Escape) {
      exitSettingsView();
      settingsViewNeedsRedraw = true;
      lastSettingsViewCursor = -1;
      return;
    }

    int settingsMovement = 0;
    if (input.event == bitmap16::InputEvent::Up) {
      settingsMovement = -1;
    } else if (input.event == bitmap16::InputEvent::Down) {
      settingsMovement = 1;
    }
    if (bitmap16::SettingsView::moveCursor(
            viewState.settings.navigation,
            settingsMovement,
            ENABLE_BLUETOOTH != 0,
            true,
            true)) {
      settingsViewNeedsRedraw = true;
    }

#if ENABLE_SCREENSHOTS
    if (input.event == bitmap16::InputEvent::Character &&
        (input.character == 'y' || input.character == 'Y')) {
      takeScreenshot();
      settingsViewNeedsRedraw = true;
    }
#endif
  }

#if ENABLE_BLUETOOTH
  // BT keyboard navigation (arrows and escape - enter handled above)
  static bool btPrevUpSettings = false, btPrevDownSettings = false;
  static bool btPrevEscSettings = false;

  if (btArrowUp && !btPrevUpSettings &&
      bitmap16::SettingsView::moveCursor(
          viewState.settings.navigation, -1, true, true, true)) {
    settingsViewNeedsRedraw = true;
  }
  if (btArrowDown && !btPrevDownSettings &&
      bitmap16::SettingsView::moveCursor(
          viewState.settings.navigation, 1, true, true, true)) {
    settingsViewNeedsRedraw = true;
  }
  if (btEscape && !btPrevEscSettings) {
    exitSettingsView();
    settingsViewNeedsRedraw = true;
    lastSettingsViewCursor = -1;
  }

  btPrevUpSettings = btArrowUp; btPrevDownSettings = btArrowDown;
  btPrevEscSettings = btEscape;
#endif
}

void drawPaletteView(bool = true) {
  if (!viewState.palette.canvasAvailable) {
    M5Cardputer.Display.fillScreen(currentTheme->background);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 55);
    M5Cardputer.Display.println("PALETTE BUFFER FAILED");
    return;
  }

  const bitmap16::PaletteView::Theme theme = {
      currentTheme->background,
      currentTheme->text,
      currentTheme->textSecondary,
      currentTheme == &THEME_DARK,
  };
  const bool showStatus =
      statusMessage[0] != '\0' &&
      millis() - statusMessageTime < STATUS_DISPLAY_DURATION;
  bitmap16::PaletteView::render(
      Display::canvas(),
      viewState.palette.navigation,
      currentPaletteCatalog(),
      activePaletteCatalogIndex(),
      theme,
      CARTRIDGE_GRAPHIC,
      showStatus ? statusMessage : nullptr);
  Display::endFrame();
}

void drawMemoryView(bool) {
  if (!viewState.memory.canvasAvailable) {
    M5Cardputer.Display.fillScreen(currentTheme->background);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 55);
    M5Cardputer.Display.println("MEMORY BUFFER FAILED");
    return;
  }
  const bitmap16::MemoryView::Theme theme = {
      currentTheme->background,
      currentTheme->cellDark,
      currentTheme->text,
      currentTheme->iconDark,
      currentTheme->iconLight,
      TFT_YELLOW,
  };
  const bitmap16::MemoryView::Assets assets = {
      ICON_SELECTOR_CORNER,
      ICON_SELECTOR_CORNER_WIDTH,
      ICON_SELECTOR_CORNER_HEIGHT,
  };
  const bool showStatus =
      statusMessage[0] != '\0' &&
      millis() - statusMessageTime < STATUS_DISPLAY_DURATION;
  bitmap16::MemoryView::render(
      Display::canvas(),
      viewState.memory.navigation,
      currentMemoryCatalog(),
      theme,
      showStatus ? statusMessage : nullptr,
      &assets);
  Display::endFrame();
}

/**
 * Draw Help Screen - displays all keyboard controls
 */
void drawHelpView() {
  if (!viewState.help.canvasAvailable) return;

  const uint32_t renderStart = micros();
  bitmap16::HelpView::Theme theme;
  theme.background = currentTheme->background;
  theme.text = currentTheme->text;
  theme.textSecondary = currentTheme->textSecondary;
  bitmap16::HelpView::render(
      Display::canvas(),
      viewState.help.navigation,
      theme,
      ENABLE_LED_MATRIX != 0,
      true);

  CanvasProofMetrics& metrics = viewState.help.metrics;
  metrics.lastRenderMicros = micros() - renderStart;
  metrics.maxRenderMicros =
      max(metrics.maxRenderMicros, metrics.lastRenderMicros);

  const uint32_t blitStart = micros();
  Display::endFrame();
  metrics.lastBlitMicros = micros() - blitStart;
  metrics.maxBlitMicros = max(metrics.maxBlitMicros, metrics.lastBlitMicros);
  metrics.frameCount++;
  metrics.minimumFreeHeap =
      min(metrics.minimumFreeHeap, static_cast<uint32_t>(ESP.getFreeHeap()));

#if ENABLE_CANVAS_PROOF_TELEMETRY
  Serial.printf(
      "[canvas-proof] frame=%lu render=%luus blit=%luus "
      "max_render=%luus max_blit=%luus min_heap=%lu\n",
      static_cast<unsigned long>(metrics.frameCount),
      static_cast<unsigned long>(metrics.lastRenderMicros),
      static_cast<unsigned long>(metrics.lastBlitMicros),
      static_cast<unsigned long>(metrics.maxRenderMicros),
      static_cast<unsigned long>(metrics.maxBlitMicros),
      static_cast<unsigned long>(metrics.minimumFreeHeap));
#endif
}

// ============================================================================
// SETUP AND LOOP
// ============================================================================

/**
 * Show boot screen with logo
 * Displays boot image from boot.png (240×135)
 */
void showBootScreen() {
  // Fill screen with black background for boot screen
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  // Display the indexed boot image with palette
  // Convert indices to RGB565 colors on-the-fly
  uint16_t* lineBuffer = (uint16_t*)malloc(240 * sizeof(uint16_t));
  if (lineBuffer) {
    for (int y = 0; y < 135; y++) {
      for (int x = 0; x < 240; x++) {
        uint8_t index = pgm_read_byte(&BOOT_IMAGE[y * 240 + x]);
        lineBuffer[x] = pgm_read_word(&BOOT_PALETTE[index]);
      }
      M5Cardputer.Display.pushImage(0, y, 240, 1, lineBuffer, 0xF81F);
    }
    free(lineBuffer);
  }

  // Display version number in lower left corner
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 135 - 12);  // 4px from left, 12px from bottom
  M5Cardputer.Display.print(FIRMWARE_VERSION);

  // Display "beepbot" centered at bottom
  int bbWidth = 7 * 6;  // 7 chars × 6px wide
  int bbX = (240 - bbWidth) / 2;
  int bbY = 135 - 12;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(bbX, bbY);
  M5Cardputer.Display.print("beepbot");

  // Display "HELP" hint in lower right corner with underlined H
  int helpX = 240 - 4 - (4 * 6);  // 4px from right, 4 chars × 6px wide
  int helpY = 135 - 12;
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(helpX, helpY);
  M5Cardputer.Display.print("HELP");
  // Underline the H (first character, 6px wide)
  M5Cardputer.Display.drawLine(helpX, helpY + 9, helpX + 5, helpY + 9, TFT_WHITE);

  // Wait for ESC key (`) press to continue, or timeout after 5 seconds
  bool waiting = true;
  unsigned long startTime = millis();
  const unsigned long timeout = 2500; // 2.5 second timeout

  while (waiting) {
    M5Cardputer.update();

    // Check for timeout
    if (millis() - startTime > timeout) {
      waiting = false;
      break;
    }

    const bitmap16::InputFrame bootInput = Input::poll(Clock::nowMs());
    if (bootInput.event == bitmap16::InputEvent::Escape) {
      waiting = false;
    }
    delay(10);  // Small delay to prevent busy-waiting
  }
  Input::reset();
}

// ============================================================================
// PALETTE SYSTEM FUNCTIONS
// ============================================================================

// Initialize stock palettes into dynamic palette arrays
void initStockPalettes() {
  for (int i = 0; i < NUM_PALETTES; i++) {
    allPalettes[i] = PALETTE_CATALOG[i];
    allPaletteNames[i] = PALETTE_NAMES[i];
    allPaletteSizes[i] = PALETTE_SIZES[i];
    paletteIsUserLoaded[i] = false;
  }
}

// Parse Lospec .hex file from SD card
// Returns true if valid palette loaded
bool loadPaletteFromHex(const char* filepath, uint16_t* colors, uint8_t* size) {
  constexpr size_t kMaxPaletteFileSize = 4096;
  const size_t fileSize = Filesystem::fileSize(filepath);
  if (fileSize == 0 || fileSize > kMaxPaletteFileSize) {
    return false;
  }

  uint8_t* contents = static_cast<uint8_t*>(malloc(fileSize));
  if (contents == nullptr) {
    return false;
  }

  size_t bytesRead = 0;
  bitmap16::Palette::Parsed parsed;
  const bool loaded =
      Filesystem::readFile(filepath, contents, fileSize, bytesRead) &&
      bitmap16::Palette::parseLospecHex(
          reinterpret_cast<const char*>(contents), bytesRead, parsed);
  free(contents);
  if (!loaded) {
    return false;
  }

  memcpy(colors, parsed.colors, sizeof(parsed.colors));
  *size = parsed.size;
  return true;
}

bool loadUserPaletteEntry(const Filesystem::FileInfo& file, void*) {
  if (file.isDirectory || totalPaletteCount >= 32) {
    return totalPaletteCount < 32;
  }

  String filename(file.name);
  const int separator = filename.lastIndexOf('/');
  if (separator >= 0) {
    filename = filename.substring(separator + 1);
  }
  if (!filename.endsWith(".hex")) {
    return true;
  }

  uint16_t* colors = static_cast<uint16_t*>(malloc(16 * sizeof(uint16_t)));
  char* name = static_cast<char*>(malloc(32));
  if (colors == nullptr || name == nullptr) {
    free(colors);
    free(name);
    return false;
  }

  uint8_t size = 0;
  String filepath = String("/bitmap16dx/palettes/") + filename;
  if (!loadPaletteFromHex(filepath.c_str(), colors, &size)) {
    free(colors);
    free(name);
    return true;
  }

  String paletteName = filename.substring(0, filename.length() - 4);
  paletteName.toUpperCase();
  paletteName.replace("-", " ");
  paletteName.replace("_", " ");
  strncpy(name, paletteName.c_str(), 31);
  name[31] = '\0';

  allPalettes[totalPaletteCount] = colors;
  allPaletteNames[totalPaletteCount] = name;
  allPaletteSizes[totalPaletteCount] = size;
  paletteIsUserLoaded[totalPaletteCount] = true;
  totalPaletteCount++;
  return totalPaletteCount < 32;
}

// Load user palettes from SD card /bitmap16dx/palettes/ folder
void loadUserPalettes() {
  if (!Filesystem::createDirectory("/bitmap16dx/palettes")) {
    return;
  }
  Filesystem::listDirectory(
      "/bitmap16dx/palettes", loadUserPaletteEntry, nullptr);
}

// Update the filtered palette list based on current filter settings
void updatePaletteFilter() {
  filteredPaletteCount = 0;

  for (uint8_t i = 0; i < totalPaletteCount; i++) {
    bool matches = true;

    // Apply size filter
    if (paletteFilterSize != 0 && allPaletteSizes[i] != paletteFilterSize) {
      matches = false;
    }

    // Apply user filter
    if (paletteFilterUser && !paletteIsUserLoaded[i]) {
      matches = false;
    }

    // Add to filtered list if matches
    if (matches) {
      filteredPaletteIndices[filteredPaletteCount++] = i;
    }
  }
}

#if ENABLE_LED_MATRIX
// ============================================================================
// LED MATRIX FUNCTIONS (8×8 WS2812 RGB LEDs)
// ============================================================================

void clearLEDMatrix() {
  LEDMatrix::clear();
  LEDMatrix::show();
}

void setLEDCell(
    uint8_t sourceGridSize,
    uint8_t x,
    uint8_t y,
    const bitmap16::LedMapping::Rgb888& color) {
  if (sourceGridSize == 8 && rgbMatrixUnits == 4) {
    for (uint8_t dy = 0; dy < 2; ++dy) {
      for (uint8_t dx = 0; dx < 2; ++dx) {
        LEDMatrix::setPixelRgb888(
            x * 2 + dx, y * 2 + dy, color.red, color.green, color.blue);
      }
    }
    return;
  }
  LEDMatrix::setPixelRgb888(
      x, y, color.red, color.green, color.blue);
}

/**
 * Update the LED matrix to mirror the current canvas.
 * Supports three display modes:
 *   1. 8×8 canvas + 1 unit → 1:1 mapping (8×8 LEDs)
 *   2. 8×8 canvas + 4 units → scaled 2× (16×16 LEDs, each pixel = 2×2 block)
 *   3. 16×16 canvas + 4 units → 1:1 mapping (16×16 LEDs)
 *
 * LED matrix is disabled when:
 *   - LED matrix setting is OFF
 *   - 16×16 canvas with only 1 unit (can't fit)
 *
 * @param showCursor If true, highlights cursor position (default). If false, shows clean canvas.
 */
void updateLEDMatrix(bool showCursor) {
  if (!LEDMatrix::isEnabled()) {
    return;
  }
  if (editorState.gridSize == 16 && rgbMatrixUnits == 1) {
    clearLEDMatrix();
    return;
  }

  for (uint8_t y = 0; y < editorState.gridSize; ++y) {
    for (uint8_t x = 0; x < editorState.gridSize; ++x) {
      const uint8_t pixelValue = editorState.canvas[y][x];
      const bool isCursor = showCursor && x == editorState.cursorX && y == editorState.cursorY;
      bitmap16::LedMapping::Rgb888 color = {};
      if (pixelValue == 0) {
        if (isCursor) {
          color = {40, 40, 40};
        }
      } else {
        color = bitmap16::LedMapping::rgb565ToRgb888(
            documentState.sketch.paletteColors[pixelValue - 1]);
        if (isCursor) {
          color.red = min(255, color.red + 80);
          color.green = min(255, color.green + 80);
          color.blue = min(255, color.blue + 80);
        }
      }
      setLEDCell(editorState.gridSize, x, y, color);
    }
  }
  LEDMatrix::show();
}

/**
 * Update the LED matrix to display a sketch (for preview mode).
 * Supports three display modes:
 *   1. 8×8 sketch + 1 unit → 1:1 mapping (8×8 LEDs)
 *   2. 8×8 sketch + 4 units → scaled 2× (16×16 LEDs, each pixel = 2×2 block)
 *   3. 16×16 sketch + 4 units → 1:1 mapping (16×16 LEDs)
 *
 * LED matrix is disabled when:
 *   - LED matrix setting is OFF
 *   - 16×16 sketch with only 1 unit (can't fit)
 *
 * Used for both canvas preview and gallery preview modes.
 */
void updateLEDMatrixFromSketch(Sketch& sketch) {
  if (!LEDMatrix::isEnabled()) {
    return;
  }
  if (sketch.gridSize == 16 && rgbMatrixUnits == 1) {
    clearLEDMatrix();
    return;
  }

  for (uint8_t y = 0; y < sketch.gridSize; ++y) {
    for (uint8_t x = 0; x < sketch.gridSize; ++x) {
      bitmap16::LedMapping::Rgb888 color = {};
      const uint8_t pixelValue = sketch.pixels[y][x];
      if (pixelValue > 0) {
        color = bitmap16::LedMapping::rgb565ToRgb888(
            sketch.paletteColors[pixelValue - 1]);
      }
      setLEDCell(sketch.gridSize, x, y, color);
    }
  }
  LEDMatrix::show();
}

/**
 * Toggle LED matrix on/off with visual feedback.
 * Shows brief status message on main display.
 */
void toggleLEDMatrix() {
    const bool enabled = !LEDMatrix::isEnabled();
    LEDMatrix::setEnabled(enabled);

    // Save preference
    PreferenceStore::writeBool("ledEnabled", enabled);

    // Visual feedback
    if (enabled) {
        // Show a brief "DX" pattern to confirm hardware is working
        // Pattern displays as a simple checkmark/confirmation graphic
        LEDMatrix::clear();

        // Set brightness to 10% for startup pattern
        LEDMatrix::setBrightness(10);

        // Define the "DX" pattern LEDs (in non-rotated coordinates)
        // Pattern:  . X X .   X . X .
        //           . X . X   . X . .
        //           . X X .   X . X .
        const int dxPattern[] = {9, 10, 17, 19, 25, 26, 36, 38, 45, 52, 54};
        const int patternSize = 11;

        // Light up the pattern with white
        // Apply the same rotation used for unit 0 to keep pattern oriented correctly
        for (int i = 0; i < patternSize; i++) {
            int index = dxPattern[i];

            // Convert linear index to x, y coordinates (8×8 grid)
            uint8_t x = index % 8;
            uint8_t y = index / 8;

            LEDMatrix::setPixelRgb888(x, y, 255, 255, 255);
        }
        LEDMatrix::show();
        delay(1000);  // Hold pattern for 1 second

        // Restore user's brightness setting
        LEDMatrix::setBrightness(ledBrightness);

        // Turn on: immediately update LEDs with current canvas
        LED_CANVAS_UPDATED();
        updateLEDMatrix();
    } else {
        // Turn off: clear all LEDs
        LEDMatrix::setEnabled(false);
    }
}

/**
 * Adjust LED matrix brightness.
 * @param delta: +1 to increase, -1 to decrease
 */
void adjustLEDBrightness(int8_t delta) {
    if (!LEDMatrix::isEnabled()) return;  // No-op if disabled

    // Adjust brightness in 1% increments
    int16_t newBrightness = ledBrightness + delta;

    // Clamp to valid range
    if (newBrightness < MIN_LED_BRIGHTNESS) {
        newBrightness = MIN_LED_BRIGHTNESS;
    } else if (newBrightness > MAX_LED_BRIGHTNESS) {
        newBrightness = MAX_LED_BRIGHTNESS;
    }

    ledBrightness = (uint8_t)newBrightness;

    LEDMatrix::setBrightness(ledBrightness);
    LEDMatrix::show();

    // Save preference
    PreferenceStore::writeUInt8("ledBright", ledBrightness);
}
#endif // ENABLE_LED_MATRIX

void runLegacyFrame();

// setup() runs once when the device boots
void setup() {
  // Initialize the M5Cardputer hardware
  // This turns on the screen, keyboard, speaker, etc.
  auto cfg = M5.config();

  // Disable auto-sleep so device stays awake during use
  cfg.internal_rtc = false;  // Disable RTC-based power management
  cfg.external_rtc = false;  // Disable external RTC if present

  M5Cardputer.begin(cfg);

  // Initialize hardware through the new platform adapters.
#if ENABLE_CANVAS_PROOF_TELEMETRY
  Serial.begin(115200);
#endif
  IMU::init();
  Indicator::init();
  Input::init();

  // Detect which Cardputer model is running
  // This helps with debugging and user support
  auto boardType = M5.getBoard();
  if (boardType == m5::board_t::board_M5Cardputer) {
    detectedBoardName = "M5Cardputer";
  } else if (boardType == m5::board_t::board_M5CardputerADV) {
    detectedBoardName = "M5Cardputer ADV";
  }

  bitmap16::Settings storedSettings;
  storedSettings.displayBrightness =
      PreferenceStore::readUInt8("brightness", 80);
  storedSettings.theme =
      PreferenceStore::readBool("darkMode", false)
          ? bitmap16::ThemeId::Dark
          : bitmap16::ThemeId::Light;
  storedSettings.defaultGridSize =
      PreferenceStore::readUInt8("defaultGrid", 8);
  storedSettings.matrixUnits =
      PreferenceStore::readUInt8("puzzleUnits", 1);
  storedSettings.matrixRotation =
      PreferenceStore::readUInt8("matrixRot", 2);
  storedSettings.exportFormat =
      PreferenceStore::readBool("exportRGB565", false)
          ? bitmap16::ExportFormat::Rgb565
          : bitmap16::ExportFormat::Rgb888;
  storedSettings.shakeUndoEnabled =
      PreferenceStore::readBool("shakeUndo", false);
  storedSettings.matrixBrightness =
      PreferenceStore::readUInt8("ledBright", DEFAULT_LED_BRIGHTNESS);
  storedSettings = bitmap16::normalizeSettings(storedSettings);

  displayBrightness = storedSettings.displayBrightness;
  currentTheme = storedSettings.theme == bitmap16::ThemeId::Dark
      ? &THEME_DARK
      : &THEME_LIGHT;
  defaultGridSize = storedSettings.defaultGridSize;
  rgbMatrixUnits = storedSettings.matrixUnits;
  matrixRotation = storedSettings.matrixRotation;
  exportRGB565 =
      storedSettings.exportFormat == bitmap16::ExportFormat::Rgb565;
  shakeUndoEnabled = storedSettings.shakeUndoEnabled;

  Display::setBrightness(displayBrightness);

#if ENABLE_LED_MATRIX
  // Initialize LED matrix (8×8 WS2812E RGB LEDs)
  const bool ledMatrixEnabled =
      PreferenceStore::readBool("ledEnabled", false);
  ledBrightness = storedSettings.matrixBrightness;

  LEDMatrix::init();
  LEDMatrix::setConfiguration(rgbMatrixUnits, matrixRotation);
  LEDMatrix::setBrightness(ledBrightness);
  LEDMatrix::setEnabled(ledMatrixEnabled);
#endif // ENABLE_LED_MATRIX

#if ENABLE_BLUETOOTH
  // Initialize Bluetooth if it was enabled
  btEnabled = PreferenceStore::readBool("btEnabled", false);
  btHasBondedDevice = PreferenceStore::readBool("btHasBonded", false);
  if (btHasBondedDevice) {
    PreferenceStore::readBytes("btBonded", btBondedAddr, 6);
  }

  // Note: BLE stack is only initialized when scanning to save memory
  // Auto-reconnect would require async scanning which blocks
  // For now, user must manually scan from Settings
#endif // ENABLE_BLUETOOTH

  // Initialize palette system
  initStockPalettes();

  // Initialize SD card and load user palettes
  if (initSDCard()) {
    loadUserPalettes();
  }

  // Show boot screen with logo
  showBootScreen();

  // Keep one shared framebuffer alive across views. Individual views reuse it.
  Display::init();
  viewState.palette.canvasAvailable = false;
  viewState.settings.canvasAvailable = false;

  // Initialize active sketch as blank
  initializeActiveSketch();

  // Create new blank sketch (will use defaultGridSize from settings)
  createNewSketch();

  // Boot directly to Draw View (not Memory View)
  drawSharedCanvasView();

  // Phase 3 lifecycle: the App now owns frame dispatch while the existing
  // handlers remain authoritative during the behavior-preserving migration.
  app.init(runLegacyFrame);
}

// ============================================================================
// VIEW HANDLER FUNCTIONS
// ============================================================================

/**
 * Handle Charging Mode - DVD-style bouncing battery icon
 */
void handleChargingMode(const bitmap16::InputFrame& input) {
  // Wait for initial key release before listening for exit
  static bool chargeWaitingForRelease = true;
  if (chargeWaitingForRelease) {
    if (!input.keyboardHeld) {
      chargeWaitingForRelease = false;
    }
    return;
  }

  // Any key press exits
  if (input.keyboardHeld) {
    chargeWaitingForRelease = true;
    exitChargingMode();
    delay(200);
    return;
  }

  // Throttle to ~30fps
  unsigned long now = millis();
  if (now - viewState.charging.lastFrameTime < CHARGE_FRAME_MS) {
    return;
  }
  viewState.charging.lastFrameTime = now;

  // Update battery level periodically
  if (now - viewState.charging.lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    viewState.charging.batteryPercent = Power::getBatteryPercent();
    viewState.charging.lastBatteryCheck = now;
    bitmap16::ChargingView::setBattery(
        viewState.charging.animation,
        viewState.charging.batteryPercent,
        chargingBatteryIcon(viewState.charging.batteryPercent));
  }

  bitmap16::ChargingView::update(
      viewState.charging.animation,
      M5Cardputer.Display.width(),
      M5Cardputer.Display.height());
  drawChargingFrame();
}

/**
 * Handle Help View input and rendering
 */
void handleHelpView(const bitmap16::InputFrame& input) {
  if (input.event == bitmap16::InputEvent::Escape ||
      (input.event == bitmap16::InputEvent::Character &&
       (input.character == 'h' || input.character == 'H'))) {
    exitHelpView();
    delay(200);
    return;
  }

#if ENABLE_SCREENSHOTS
  if (input.event == bitmap16::InputEvent::Character &&
      (input.character == 'y' || input.character == 'Y')) {
    takeScreenshot();
    drawHelpView();
  }
#endif

  int helpMovement = 0;
  if (input.event == bitmap16::InputEvent::Up) {
    helpMovement = -1;
  } else if (input.event == bitmap16::InputEvent::Down) {
    helpMovement = 1;
  }
  if (bitmap16::HelpView::moveCursor(
          viewState.help.navigation,
          helpMovement,
          ENABLE_LED_MATRIX != 0,
          true)) {
    drawHelpView();
  }

#if ENABLE_BLUETOOTH
  static bool btPrevEscHelp = false;
  static bool btPrevUpHelp = false;
  static bool btPrevDownHelp = false;

  if (btEscape && !btPrevEscHelp) {
    exitHelpView();
  }
  btPrevEscHelp = btEscape;

  if (btUp && !btPrevUpHelp &&
      bitmap16::HelpView::moveCursor(
          viewState.help.navigation, -1, ENABLE_LED_MATRIX != 0, true)) {
    drawHelpView();
  }
  btPrevUpHelp = btUp;

  if (btDown && !btPrevDownHelp &&
      bitmap16::HelpView::moveCursor(
          viewState.help.navigation, 1, ENABLE_LED_MATRIX != 0, true)) {
    drawHelpView();
  }
  btPrevDownHelp = btDown;

  char btChar;
  while ((btChar = btQueuePop()) != '\0') {
    if (btChar == 'h' || btChar == 'H') {
      exitHelpView();
      return;
    }
  }
#endif

  delay(10);
}

/**
 * Handle Memory View input and rendering
 */
void handleMemoryView(const bitmap16::InputFrame& input) {
  // Handle memory view controls
  static bool memoryViewNeedsRedraw = true;
  static int lastMemoryViewCursor = -1;

  {
    unsigned long now = millis();
    if (now - viewState.memory.lastAnimationTime < MEMORY_ANIM_FRAME_MS) {
    } else {
      unsigned long deltaTime = now - viewState.memory.lastAnimationTime;
      if (deltaTime > 100) deltaTime = 100;
      bitmap16::MemoryView::advance(
          viewState.memory.navigation,
          sketchList.size(),
          M5Cardputer.Display.width(),
          M5Cardputer.Display.height(),
          static_cast<float>(deltaTime) / 1000.0f,
          MEMORY_SCROLL_SPEED);
      drawMemoryView(memoryViewNeedsRedraw);
      memoryViewNeedsRedraw = false;
      lastMemoryViewCursor = viewState.memory.navigation.cursor;
      viewState.memory.lastAnimationTime = now;
    }
  }

  // Check for G0 button - delete selected sketch (if not on "+")
  if (input.actionPressed && viewState.memory.navigation.cursor > 0) {
    int sketchIndex = viewState.memory.navigation.cursor - 1;
    if (sketchIndex < sketchList.size()) {
      // Save sketch to undo buffer before deleting (so we can restore with Z)
      Sketch& sketchData = sketchList[sketchIndex].sketchData;

      // Copy pixel data to undo buffer
      for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
          editorState.undoCanvas[y][x] = sketchData.pixels[y][x];
        }
      }

      // Copy palette and grid info to undo buffer
      for (int i = 0; i < 16; i++) {
        editorState.undoPaletteColors[i] = sketchData.paletteColors[i];
      }
      editorState.undoPaletteSize = sketchData.paletteSize;
      editorState.undoGridSize = sketchData.gridSize;
      editorState.undoAvailable = true;

      // Now delete the file
      String filename = "/bitmap16dx/sketches/" + sketchList[sketchIndex].filename;
      Filesystem::remove(filename.c_str());
      loadSketchListFromSD();  // Refresh list (clears cached data)

      // Move cursor if we deleted the last item
      int totalItems = 1 + sketchList.size();
      if (viewState.memory.navigation.cursor >= totalItems) {
        viewState.memory.navigation.cursor = totalItems - 1;
      }
    }
    memoryViewNeedsRedraw = true;
    lastMemoryViewCursor = -1;
  }

  if (input.enterPressed) {
    if (viewState.memory.navigation.cursor == 0) {
      // Create new blank sketch
      createNewSketch();
    } else {
      // Open selected sketch
      int sketchIndex = viewState.memory.navigation.cursor - 1;
      if (sketchIndex < sketchList.size()) {
        openSketch(sketchList[sketchIndex].filename);
      }
    }
    exitMemoryView();
    memoryViewNeedsRedraw = true;
    lastMemoryViewCursor = -1;
    return;
  }

  {
    const char command =
        input.event == bitmap16::InputEvent::Character
            ? input.character
            : '\0';
    // Z key - Undo (restore last cleared sketch from memory view)
    if (command == 'z' || command == 'Z') {
        if (editorState.undoAvailable) {
          // Restore the undo buffer to active sketch
          // (This restores canvas-level undo, not sketch deletion)

          // If we have saved grid size info, restore it
          if (editorState.undoGridSize > 0) {
            editorState.gridSize = editorState.undoGridSize;
            editorState.cellSize = (editorState.gridSize == 8) ? 16 : 8;

            // Keep cursor in bounds
            if (editorState.cursorX >= editorState.gridSize) editorState.cursorX = editorState.gridSize - 1;
            if (editorState.cursorY >= editorState.gridSize) editorState.cursorY = editorState.gridSize - 1;
          }

          // Restore pixel data to canvas
          for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
              editorState.canvas[y][x] = editorState.undoCanvas[y][x];
            }
          }

          // Restore palette information to active sketch
          documentState.sketch.paletteSize = editorState.undoPaletteSize;
          documentState.sketch.gridSize = editorState.undoGridSize;
          for (int i = 0; i < 16; i++) {
            documentState.sketch.paletteColors[i] = editorState.undoPaletteColors[i];
          }

          documentState.sketch.isEmpty = false;
          editorState.undoAvailable = false;

          // Save restored sketch to SD card (now uses active sketch system)
          // Copy canvas to active sketch before saving
          for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
              documentState.sketch.pixels[y][x] = editorState.canvas[y][x];
            }
          }
          documentState.sketch.gridSize = editorState.gridSize;
          saveActiveSketchToSD();

          // Reload sketch list to show the restored sketch
          loadSketchListFromSD();

          setStatusMessage(StatusMsg::RESTORED_SKETCH);
          memoryViewNeedsRedraw = true;
          lastMemoryViewCursor = -1;
        } else {
          setStatusMessage(StatusMsg::NO_UNDO);
        }
      }
      // ` key (ESC) or O key - exit memory view
    else if (input.event == bitmap16::InputEvent::Escape ||
             command == 'o' || command == 'O') {
        // Always allow exiting memory view (can go back to current sketch)
        exitMemoryView();
        memoryViewNeedsRedraw = true;
        lastMemoryViewCursor = -1;
        return;
      }
    // H key - Open help view
    else if (command == 'h' || command == 'H') {
        enterHelpView();
        return;  // Exit memory view loop to enter help view mode
      }
      // V key - View selected sketch in gallery preview
    else if (command == 'v' || command == 'V') {
        if (sketchList.size() > 0) {
          enterPreviewView();  // Detects Memory View and enables gallery mode
          return;
        } else {
          setStatusMessage("No sketches to show");
        }
      }
#if ENABLE_SCREENSHOTS
      // Y key - Take Screenshot
    else if (command == 'y' || command == 'Y') {
        takeScreenshot();
        memoryViewNeedsRedraw = true;  // Redraw after screenshot status message
      }
#endif
    int deltaX = 0;
    int deltaY = 0;
    if (input.event == bitmap16::InputEvent::Up) deltaY = -1;
    if (input.event == bitmap16::InputEvent::Down) deltaY = 1;
    if (input.event == bitmap16::InputEvent::Left) deltaX = -1;
    if (input.event == bitmap16::InputEvent::Right) deltaX = 1;
    bitmap16::MemoryView::moveCursor(
        viewState.memory.navigation,
        deltaX,
        deltaY,
        sketchList.size(),
        M5Cardputer.Display.width());
  }

#if ENABLE_BLUETOOTH
  // BT keyboard navigation for memory view
  static bool btPrevUpMem = false, btPrevDownMem = false;
  static bool btPrevLeftMem = false, btPrevRightMem = false;
  static bool btPrevEnterMem = false, btPrevEscMem = false;

  if (btArrowUp && !btPrevUpMem) {
    bitmap16::MemoryView::moveCursor(
        viewState.memory.navigation,
        0,
        -1,
        sketchList.size(),
        M5Cardputer.Display.width());
  }
  if (btArrowDown && !btPrevDownMem) {
    bitmap16::MemoryView::moveCursor(
        viewState.memory.navigation,
        0,
        1,
        sketchList.size(),
        M5Cardputer.Display.width());
  }
  if (btArrowLeft && !btPrevLeftMem) {
    bitmap16::MemoryView::moveCursor(
        viewState.memory.navigation,
        -1,
        0,
        sketchList.size(),
        M5Cardputer.Display.width());
  }
  if (btArrowRight && !btPrevRightMem) {
    bitmap16::MemoryView::moveCursor(
        viewState.memory.navigation,
        1,
        0,
        sketchList.size(),
        M5Cardputer.Display.width());
  }
  if (btEnter && !btPrevEnterMem) {
    if (viewState.memory.navigation.cursor == 0) {
      createNewSketch();
    } else {
      int sketchIndex = viewState.memory.navigation.cursor - 1;
      if (sketchIndex < sketchList.size()) {
        openSketch(sketchList[sketchIndex].filename);
      }
    }
    exitMemoryView();
    memoryViewNeedsRedraw = true;
  }
  if (btEscape && !btPrevEscMem) {
    exitMemoryView();
    memoryViewNeedsRedraw = true;
  }

  btPrevUpMem = btArrowUp; btPrevDownMem = btArrowDown;
  btPrevLeftMem = btArrowLeft; btPrevRightMem = btArrowRight;
  btPrevEnterMem = btEnter; btPrevEscMem = btEscape;
#endif

  delay(10);
}

/**
 * Handle Preview View input and rendering
 */
void handlePreviewView(const bitmap16::InputFrame& input) {
  // Auto-advance logic (ONLY in gallery mode)
  if (viewState.preview.galleryMode && viewState.preview.autoAdvance) {
    unsigned long now = millis();
    if (now - viewState.preview.lastAdvanceTime >= GALLERY_ADVANCE_INTERVAL) {
      // Advance to next sketch
      viewState.preview.galleryIndex++;
      if (viewState.preview.galleryIndex >= sketchList.size()) {
        viewState.preview.galleryIndex = 0;  // Wrap to start
      }
      viewState.preview.lastAdvanceTime = now;
      loadGallerySketch(viewState.preview.galleryIndex);  // Redraw with new sketch
    }
  }

  const bool characterEvent =
      input.event == bitmap16::InputEvent::Character;
  if (input.event == bitmap16::InputEvent::Escape ||
      (characterEvent && (input.character == 'v' || input.character == 'V'))) {
    exitPreviewView();
    delay(200);
    return;
  }

  if (viewState.preview.galleryMode && input.event == bitmap16::InputEvent::Left) {
    --viewState.preview.galleryIndex;
    if (viewState.preview.galleryIndex < 0) {
      viewState.preview.galleryIndex = sketchList.size() - 1;
    }
    viewState.preview.autoAdvance = false;
    loadGallerySketch(viewState.preview.galleryIndex);
  } else if (viewState.preview.galleryMode && input.event == bitmap16::InputEvent::Right) {
    ++viewState.preview.galleryIndex;
    if (viewState.preview.galleryIndex >= sketchList.size()) {
      viewState.preview.galleryIndex = 0;
    }
    viewState.preview.autoAdvance = false;
    loadGallerySketch(viewState.preview.galleryIndex);
  } else if (viewState.preview.galleryMode && input.event == bitmap16::InputEvent::Space) {
    viewState.preview.autoAdvance = !viewState.preview.autoAdvance;
    if (viewState.preview.autoAdvance) {
      viewState.preview.lastAdvanceTime = millis();
    }
  } else {
    int requestedBackground = -1;
    switch (input.event) {
      case bitmap16::InputEvent::Number1:
        requestedBackground = 0;
        break;
      case bitmap16::InputEvent::Number2:
        requestedBackground = 1;
        break;
      case bitmap16::InputEvent::Number3:
        requestedBackground = 2;
        break;
      case bitmap16::InputEvent::Number4:
        requestedBackground = 3;
        break;
      default:
        break;
    }

    if (requestedBackground >= 0) {
      bitmap16::PreviewView::selectBackground(
          viewState.preview.display, requestedBackground);
      if (viewState.preview.galleryMode) {
        loadGallerySketch(viewState.preview.galleryIndex);
      } else {
        drawCanvasPreview();
      }
    } else if (input.bHeld &&
               (input.event == bitmap16::InputEvent::Plus ||
                input.event == bitmap16::InputEvent::Minus)) {
      const int BRIGHTNESS_STEP = 10;
      const int MIN_BRIGHTNESS = 10;
      const int MAX_BRIGHTNESS = 100;

      if (input.event == bitmap16::InputEvent::Plus) {
        displayBrightness =
            min(MAX_BRIGHTNESS, displayBrightness + BRIGHTNESS_STEP);
      } else {
        displayBrightness =
            max(MIN_BRIGHTNESS, displayBrightness - BRIGHTNESS_STEP);
      }

      Display::setBrightness(displayBrightness);

      PreferenceStore::writeUInt8("brightness", displayBrightness);

      char brightnessMsg[20];
      snprintf(
          brightnessMsg,
          sizeof(brightnessMsg),
          "BRIGHT: %d%%",
          displayBrightness);
      setStatusMessage(brightnessMsg);

      if (viewState.preview.galleryMode) {
        loadGallerySketch(viewState.preview.galleryIndex);
      } else {
        drawCanvasPreview();
      }
    }
#if ENABLE_SCREENSHOTS
    else if (characterEvent &&
             (input.character == 'y' || input.character == 'Y')) {
      takeScreenshot();
      if (viewState.preview.galleryMode) {
        loadGallerySketch(viewState.preview.galleryIndex);
      } else {
        drawCanvasPreview();
      }
    }
#endif
  }

#if ENABLE_BLUETOOTH
  // BT keyboard navigation for preview/gallery view
  static bool btPrevLeftPrev = false, btPrevRightPrev = false;
  static bool btPrevEscPrev = false;

  if (btEscape && !btPrevEscPrev) {
    exitPreviewView();
  }
  if (viewState.preview.galleryMode) {
    if (btArrowLeft && !btPrevLeftPrev) {
      viewState.preview.galleryIndex--;
      if (viewState.preview.galleryIndex < 0) viewState.preview.galleryIndex = sketchList.size() - 1;
      viewState.preview.autoAdvance = false;
      loadGallerySketch(viewState.preview.galleryIndex);
    }
    if (btArrowRight && !btPrevRightPrev) {
      viewState.preview.galleryIndex++;
      if (viewState.preview.galleryIndex >= sketchList.size()) viewState.preview.galleryIndex = 0;
      viewState.preview.autoAdvance = false;
      loadGallerySketch(viewState.preview.galleryIndex);
    }
  }

  btPrevLeftPrev = btArrowLeft; btPrevRightPrev = btArrowRight;
  btPrevEscPrev = btEscape;
#endif

  delay(10);
}

/**
 * Handle Palette View input and rendering
 */
void handlePaletteView(const bitmap16::InputFrame& input) {
  static bool paletteViewNeedsRedraw = true;
  static int lastPaletteViewCursor = -1;

  const bitmap16::PaletteView::AnimationResult animation =
      bitmap16::PaletteView::advance(
          viewState.palette.navigation,
          PALETTE_SCROLL_SPEED,
          PALETTE_INSERT_SPEED);
  if (animation == bitmap16::PaletteView::AnimationResult::SelectionComplete) {
    drawPaletteView(false);
    delay(500);
    viewState.palette.navigation.insertionAnimating = false;
    exitPaletteView();
    paletteViewNeedsRedraw = true;
    lastPaletteViewCursor = -1;
    return;
  }
  if (animation == bitmap16::PaletteView::AnimationResult::Animating) {
    const unsigned long now = millis();
    if (now - viewState.palette.lastAnimationTime >=
        PALETTE_ANIM_FRAME_MS) {
      drawPaletteView(false);
      viewState.palette.lastAnimationTime = now;
    }
  } else if (
      paletteViewNeedsRedraw ||
      lastPaletteViewCursor != viewState.palette.navigation.cursor) {
    drawPaletteView(true);
    paletteViewNeedsRedraw = false;
    lastPaletteViewCursor = viewState.palette.navigation.cursor;
  }

  const bool characterEvent =
      input.event == bitmap16::InputEvent::Character;
  if (input.event == bitmap16::InputEvent::Escape ||
      (characterEvent && (input.character == 'p' || input.character == 'P'))) {
    exitPaletteView();
    paletteViewNeedsRedraw = true;
    lastPaletteViewCursor = -1;
    delay(200);
    return;
  }

  const bitmap16::PaletteView::Catalog catalog = currentPaletteCatalog();
  if (input.enterPressed &&
      bitmap16::PaletteView::beginSelection(
          viewState.palette.navigation)) {
    const int selectedPaletteIdx =
        bitmap16::PaletteView::selectedCatalogIndex(
            viewState.palette.navigation);
    documentState.sketch.paletteSize = allPaletteSizes[selectedPaletteIdx];
    for (int i = 0; i < 16; ++i) {
      documentState.sketch.paletteColors[i] =
          pgm_read_word(&allPalettes[selectedPaletteIdx][i]);
    }
    LED_CANVAS_UPDATED();
  } else if (input.event == bitmap16::InputEvent::Number0) {
    bitmap16::PaletteView::reset(viewState.palette.navigation, catalog);
    paletteViewNeedsRedraw = true;
  } else if (input.event == bitmap16::InputEvent::Number4 ||
             input.event == bitmap16::InputEvent::Number8 ||
             input.event == bitmap16::InputEvent::Number1) {
    uint8_t requestedSize = 16;
    if (input.event == bitmap16::InputEvent::Number4) {
      requestedSize = 4;
    } else if (input.event == bitmap16::InputEvent::Number8) {
      requestedSize = 8;
    }

    bitmap16::PaletteView::toggleSizeFilter(
        viewState.palette.navigation, catalog, requestedSize);
    paletteViewNeedsRedraw = true;
  } else if (characterEvent &&
             (input.character == 'u' || input.character == 'U')) {
    bitmap16::PaletteView::toggleUserFilter(
        viewState.palette.navigation, catalog);
    paletteViewNeedsRedraw = true;
  } else if (input.event == bitmap16::InputEvent::Left) {
    bitmap16::PaletteView::moveCursor(viewState.palette.navigation, -1);
  } else if (input.event == bitmap16::InputEvent::Right) {
    bitmap16::PaletteView::moveCursor(viewState.palette.navigation, 1);
  }
#if ENABLE_SCREENSHOTS
  else if (characterEvent &&
           (input.character == 'y' || input.character == 'Y')) {
    takeScreenshot();
    paletteViewNeedsRedraw = true;
  }
#endif

#if ENABLE_BLUETOOTH
  // BT keyboard navigation for palette view
  static bool btPrevLeftPal = false, btPrevRightPal = false;
  static bool btPrevEnterPal = false, btPrevEscPal = false;

  if (btArrowLeft && !btPrevLeftPal) {
    bitmap16::PaletteView::moveCursor(
        viewState.palette.navigation, -1);
  }
  if (btArrowRight && !btPrevRightPal) {
    bitmap16::PaletteView::moveCursor(
        viewState.palette.navigation, 1);
  }
  if (btEnter && !btPrevEnterPal &&
      bitmap16::PaletteView::beginSelection(
          viewState.palette.navigation)) {
    const int selectedPaletteIdx =
        bitmap16::PaletteView::selectedCatalogIndex(
            viewState.palette.navigation);
    documentState.sketch.paletteSize = allPaletteSizes[selectedPaletteIdx];
    for (int i = 0; i < 16; i++) {
      documentState.sketch.paletteColors[i] = pgm_read_word(&allPalettes[selectedPaletteIdx][i]);
    }
    LED_CANVAS_UPDATED();
  }
  if (btEscape && !btPrevEscPal) {
    exitPaletteView();
    paletteViewNeedsRedraw = true;
    lastPaletteViewCursor = -1;
  }

  btPrevLeftPal = btArrowLeft; btPrevRightPal = btArrowRight;
  btPrevEnterPal = btEnter; btPrevEscPal = btEscape;
#endif

  delay(10);
}

/**
 * Handle Canvas View input and rendering
 */
void handleCanvasView(const bitmap16::InputFrame& input);

#if ENABLE_BLUETOOTH
// ============================================================================
// BLUETOOTH KEYBOARD SUPPORT FUNCTIONS
// ============================================================================

// NimBLE callback classes
class BtClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) override {
    btConnected = true;
    btShowNotify("BT Connected");
  }

  void onDisconnect(NimBLEClient* pClient) override {
    btConnected = false;
    btClearInputState();
    btShowNotify("BT Disconnected");
  }
};

static BtClientCallbacks btClientCallbacks;

// Notification callback for HID reports
void btNotifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  btProcessHIDReport(pData, length);
}

// Track if BLE stack is initialized
static bool btStackInitialized = false;

/**
 * Initialize NimBLE stack
 */
void btInit() {
  if (btStackInitialized) return;
  NimBLEDevice::init("BitMap16 DX");
  NimBLEDevice::setSecurityAuth(true, false, true);  // bonding, no MITM, secure conn
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power for better range
  btStackInitialized = true;
}

/**
 * Deinitialize NimBLE stack to free memory (~60-80KB)
 */
void btDeinit() {
  if (!btStackInitialized) return;
  if (btConnected) return;  // Don't deinit while connected

  if (btClient) {
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
  }
  if (btAdvDevice) {
    delete btAdvDevice;
    btAdvDevice = nullptr;
  }

  NimBLEDevice::deinit(true);  // true = release memory
  btStackInitialized = false;
}

/**
 * Start scanning for HID keyboards
 */
void btStartScan() {
  if (btScanning) return;

  btScanning = true;

  // Initialize BLE stack if needed
  btInit();

  // Redraw settings view to show [SCANNING] before blocking
  if (app.currentView() == bitmap16::ViewId::Settings) {
    drawSettingsView();
  }

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);

  // Scan with countdown display
  const int SCAN_SECONDS = 15;
  for (int remaining = SCAN_SECONDS; remaining > 0; remaining--) {
    // Update countdown and redraw
    btScanCountdown = remaining;
    if (app.currentView() == bitmap16::ViewId::Settings) {
      drawSettingsView();
    }

    // Scan for 1 second
    pScan->start(1, false);
  }
  btScanCountdown = 0;

  NimBLEScanResults results = pScan->getResults();

  // Look for HID devices - try multiple detection methods
  // Clean up previous device if any
  if (btAdvDevice) {
    delete btAdvDevice;
    btAdvDevice = nullptr;
  }
  int deviceCount = results.getCount();
  btLastScanCount = deviceCount;  // Store for settings UI

  for (int i = 0; i < deviceCount; i++) {
    NimBLEAdvertisedDevice device = results.getDevice(i);

    // Method 1: Check for HID service UUID
    if (device.isAdvertisingService(NimBLEUUID((uint16_t)0x1812))) {
      btAdvDevice = new NimBLEAdvertisedDevice(device);
      break;
    }

    // Method 2: Check device appearance (0x03C1 = 961 = keyboard)
    if (device.getAppearance() == 961) {
      btAdvDevice = new NimBLEAdvertisedDevice(device);
      break;
    }

    // Method 3: Check if name contains keyboard-related strings (case insensitive)
    if (device.haveName()) {
      std::string name = device.getName();
      // Convert to lowercase for comparison
      for (auto& c : name) c = tolower(c);
      if (name.find("keyboard") != std::string::npos ||
          name.find("kbd") != std::string::npos ||
          name.find("keychron") != std::string::npos ||
          name.find("k8") != std::string::npos ||
          name.find("k1") != std::string::npos ||
          name.find("k2") != std::string::npos ||
          name.find("k3") != std::string::npos ||
          name.find("k4") != std::string::npos ||
          name.find("k6") != std::string::npos ||
          name.find("logitech") != std::string::npos ||
          name.find("magic") != std::string::npos) {
        btAdvDevice = new NimBLEAdvertisedDevice(device);
        break;
      }
    }
  }

  pScan->clearResults();
  btScanning = false;

  if (btAdvDevice) {
    btShowNotify("Found keyboard");
  } else {
    char msg[32];
    snprintf(msg, sizeof(msg), "No kbd (%d found)", deviceCount);
    btShowNotify(msg);
  }
}

/**
 * Connect to discovered HID keyboard
 */
bool btConnect() {
  if (!btAdvDevice) return false;
  if (btConnected) return true;

  btShowNotify("Connecting...");

  // Create client
  btClient = NimBLEDevice::createClient();
  btClient->setClientCallbacks(&btClientCallbacks, false);
  btClient->setConnectionParams(12, 12, 0, 150);
  btClient->setConnectTimeout(10);

  // Connect to device
  if (!btClient->connect(btAdvDevice)) {
    btShowNotify("Connect failed");
    return false;
  }

  // Secure the connection
  if (!btClient->secureConnection()) {
    btClient->disconnect();
    btShowNotify("Pairing failed");
    return false;
  }

  // Find HID service
  NimBLERemoteService* hidService = btClient->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hidService) {
    btClient->disconnect();
    btShowNotify("No HID service");
    return false;
  }

  // Find and subscribe to HID report characteristics
  // Try standard report characteristic first (0x2A4D)
  std::vector<NimBLERemoteCharacteristic*>* chars = hidService->getCharacteristics(true);
  bool subscribed = false;

  for (auto& chr : *chars) {
    if (chr->getUUID() == NimBLEUUID((uint16_t)0x2A4D)) {
      if (chr->canNotify()) {
        chr->subscribe(true, btNotifyCallback);
        subscribed = true;
      }
    }
  }

  // Also try boot keyboard input (0x2A22)
  NimBLERemoteCharacteristic* bootChar = hidService->getCharacteristic(NimBLEUUID((uint16_t)0x2A22));
  if (bootChar && bootChar->canNotify()) {
    bootChar->subscribe(true, btNotifyCallback);
    subscribed = true;
  }

  if (!subscribed) {
    btClient->disconnect();
    btShowNotify("Subscribe failed");
    return false;
  }

  // Store bonded device address for auto-reconnect
  memcpy(btBondedAddr, btAdvDevice->getAddress().getNative(), 6);
  btHasBondedDevice = true;

  // Clean up - we don't need the advertised device anymore
  delete btAdvDevice;
  btAdvDevice = nullptr;

  return true;
}

/**
 * Reconnect to previously bonded keyboard (no scan needed)
 */
bool btReconnect() {
  if (!btHasBondedDevice) return false;
  if (btConnected) return true;

  // Initialize BLE stack if needed
  btInit();

  btShowNotify("Reconnecting...");

  // Create client
  btClient = NimBLEDevice::createClient();
  btClient->setClientCallbacks(&btClientCallbacks, false);
  btClient->setConnectionParams(12, 12, 0, 150);
  btClient->setConnectTimeout(10);

  // Connect directly to bonded address
  NimBLEAddress addr(btBondedAddr);
  if (!btClient->connect(addr)) {
    btShowNotify("Reconnect failed");
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
    btDeinit();
    return false;
  }

  // Secure the connection
  if (!btClient->secureConnection()) {
    btClient->disconnect();
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
    btShowNotify("Pairing failed");
    btDeinit();
    return false;
  }

  // Find HID service
  NimBLERemoteService* hidService = btClient->getService(NimBLEUUID((uint16_t)0x1812));
  if (!hidService) {
    btClient->disconnect();
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
    btShowNotify("No HID service");
    btDeinit();
    return false;
  }

  // Find and subscribe to HID report characteristics
  std::vector<NimBLERemoteCharacteristic*>* chars = hidService->getCharacteristics(true);
  bool subscribed = false;

  for (auto& chr : *chars) {
    if (chr->getUUID() == NimBLEUUID((uint16_t)0x2A4D)) {
      if (chr->canNotify()) {
        chr->subscribe(true, btNotifyCallback);
        subscribed = true;
      }
    }
  }

  // Also try boot keyboard input
  NimBLERemoteCharacteristic* bootChar = hidService->getCharacteristic(NimBLEUUID((uint16_t)0x2A22));
  if (bootChar && bootChar->canNotify()) {
    bootChar->subscribe(true, btNotifyCallback);
    subscribed = true;
  }

  if (!subscribed) {
    btClient->disconnect();
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
    btShowNotify("Subscribe failed");
    btDeinit();
    return false;
  }

  return true;
}

/**
 * Disconnect from keyboard
 */
void btDisconnect() {
  if (btClient) {
    if (btClient->isConnected()) {
      btClient->disconnect();
    }
    NimBLEDevice::deleteClient(btClient);
    btClient = nullptr;
  }
  if (btAdvDevice) {
    delete btAdvDevice;
    btAdvDevice = nullptr;
  }
  btConnected = false;
  btClearInputState();
}

/**
 * Process incoming HID keyboard report
 * Standard HID keyboard report format:
 * Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI)
 * Byte 1: Reserved (always 0)
 * Bytes 2-7: Up to 6 simultaneous key codes
 */
void btProcessHIDReport(uint8_t* data, size_t len) {
  if (len < 3) return;

  // Normalize to 8-byte report
  uint8_t report[8] = {0};
  memcpy(report, data, min(len, (size_t)8));

  // Check modifier keys (byte 0)
  bool shift = (report[0] & 0x22) != 0;  // Left or right shift
  btFnHeld = (report[0] & 0x44) != 0;    // Left or right Alt = Fn

  // Clear directional state (will be set if keys are held)
  btArrowUp = false;
  btArrowDown = false;
  btArrowLeft = false;
  btArrowRight = false;
  btEnter = false;
  btBackspace = false;
  btEscape = false;
  btSpace = false;
  btFill = false;

  // Process key codes (bytes 2-7)
  for (int i = 2; i < 8; i++) {
    uint8_t keycode = report[i];
    if (keycode == 0) continue;

    // Check if this is a new key press (not in previous report)
    bool isNewPress = true;
    for (int j = 2; j < 8; j++) {
      if (btPrevReport[j] == keycode) {
        isNewPress = false;
        break;
      }
    }

    // Handle special keys (always update state for held keys)
    switch (keycode) {
      case HID_KEY_UP_ARROW:    btArrowUp = true; break;
      case HID_KEY_DOWN_ARROW:  btArrowDown = true; break;
      case HID_KEY_LEFT_ARROW:  btArrowLeft = true; break;
      case HID_KEY_RIGHT_ARROW: btArrowRight = true; break;
      case HID_KEY_ENTER:       btEnter = true; break;
      case HID_KEY_BACKSPACE:   btBackspace = true; break;
      case HID_KEY_ESCAPE:      btEscape = true; break;
      case HID_KEY_SPACE:       btSpace = true; break;
      case HID_KEY_F:
        btFill = true;
        if (isNewPress) btQueuePush(shift ? 'F' : 'f');
        break;
      default:
        // For character keys, only queue on new press
        if (isNewPress) {
          char c = btHidToChar(keycode, shift);
          if (c != 0) {
            btQueuePush(c);
          }
        }
        break;
    }
  }

  // Save current report for next comparison
  memcpy(btPrevReport, report, 8);
}

/**
 * Convert HID keycode to ASCII character
 * Simplified US keyboard layout
 */
char btHidToChar(uint8_t keycode, bool shift) {
  // Letters (0x04-0x1D = a-z)
  if (keycode >= 0x04 && keycode <= 0x1D) {
    char c = 'a' + (keycode - 0x04);
    return shift ? (c - 32) : c;  // Convert to uppercase if shift
  }

  // Numbers (0x1E-0x27 = 1-9, 0)
  if (keycode >= 0x1E && keycode <= 0x27) {
    if (shift) {
      // Shifted number row symbols
      const char* symbols = "!@#$%^&*()";
      return symbols[keycode - 0x1E];
    }
    if (keycode == 0x27) return '0';
    return '1' + (keycode - 0x1E);
  }

  // Common punctuation
  switch (keycode) {
    case 0x2C: return ' ';   // Space
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
  }

  return 0;  // Unknown keycode
}

/**
 * Push character to input queue
 */
void btQueuePush(char c) {
  uint8_t nextHead = (btQueueHead + 1) % BT_QUEUE_SIZE;
  if (nextHead != btQueueTail) {  // Not full
    btInputQueue[btQueueHead] = c;
    btQueueHead = nextHead;
  }
}

/**
 * Pop character from input queue
 */
bool btQueuePop(char& c) {
  if (btQueueHead == btQueueTail) return false;  // Empty
  c = btInputQueue[btQueueTail];
  btQueueTail = (btQueueTail + 1) % BT_QUEUE_SIZE;
  return true;
}

/**
 * Clear all BT input state
 */
void btClearInputState() {
  btArrowUp = btArrowDown = btArrowLeft = btArrowRight = false;
  btEnter = btBackspace = btEscape = btSpace = btFill = btFnHeld = false;
  btQueueHead = btQueueTail = 0;
  memset(btPrevReport, 0, 8);
}

/**
 * Show notification message (flashes briefly)
 */
void btShowNotify(const char* msg) {
  strncpy(btNotifyMsg, msg, sizeof(btNotifyMsg) - 1);
  btNotifyMsg[sizeof(btNotifyMsg) - 1] = '\0';
  btNotifyTime = millis();
}

/**
 * Update/clear notification display
 */
void btUpdateNotify() {
  if (btNotifyTime > 0 && millis() - btNotifyTime > BT_NOTIFY_DURATION) {
    btNotifyTime = 0;
    btNotifyMsg[0] = '\0';
  }
}
#endif // ENABLE_BLUETOOTH

// ============================================================================
// SHAKE DETECTION
// ============================================================================

/**
 * Detect shake gesture using IMU accelerometer
 *
 * Reads accelerometer data from the BMI270 sensor and calculates total
 * acceleration magnitude. A shake is detected when acceleration exceeds
 * the threshold and the cooldown period has elapsed.
 *
 * How it works:
 * 1. Get IMU data (x, y, z acceleration in G-forces)
 * 2. Calculate magnitude using: sqrt(x² + y² + z²)
 * 3. Check if magnitude exceeds threshold (6G)
 * 4. Ensure cooldown period has passed (500ms)
 * 5. Return true if shake detected
 *
 * The threshold of 6G is calibrated to:
 * - Be intentional (won't trigger from gentle movements)
 * - Not be exhausting (doesn't require violent shaking)
 * - Feel natural for a handheld device
 *
 * @return true if shake gesture detected, false otherwise
 */
bool detectShakeGesture() {
  return shakeDetector.update(IMU::readAcceleration(), Clock::nowMs());
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  app.tick();
}

void runLegacyFrame() {
  // Update the M5 hardware state (this checks for keyboard input)
  M5Cardputer.update();

  // Poll once so migrated and legacy handlers observe the same hardware frame.
  const bitmap16::InputFrame input = Input::poll(Clock::nowMs());

#if ENABLE_BLUETOOTH
  // Update BT notification display timer
  btUpdateNotify();
#endif

  // ============================================================================
  // SHAKE-TO-UNDO DETECTION (IMU)
  // ============================================================================
  // Check for shake gesture ONLY in canvas view
  if (app.currentView() == bitmap16::ViewId::Canvas) {
    if (shakeUndoEnabled && detectShakeGesture() && editorState.undoAvailable) {
      // Shake detected and undo is available!
      restoreUndo();  // Perform the undo operation
      drawSharedCanvasView();
    }
  }

  // App ViewId is authoritative for frame dispatch.
  switch (app.currentView()) {
    case bitmap16::ViewId::Charging:
      handleChargingMode(input);
      break;
    case bitmap16::ViewId::Help:
      handleHelpView(input);
      break;
    case bitmap16::ViewId::Memory:
      handleMemoryView(input);
      break;
    case bitmap16::ViewId::Preview:
      handlePreviewView(input);
      break;
    case bitmap16::ViewId::Palette:
      handlePaletteView(input);
      break;
    case bitmap16::ViewId::Settings:
      handleSettingsView(input);
      break;
    case bitmap16::ViewId::Canvas:
      handleCanvasView(input);
      break;
  }
}

void handleCanvasView(const bitmap16::InputFrame& input) {
  // Track what changed for redrawing
  bool moved = false;
  bool pixelPlaced = false;
  bool colorChanged = false;
  bool canvasCleared = false;
  bool undoPerformed = false;
  bool gridToggled = false;
  bool rulersToggled = false;
  bool themeToggled = false;
  bool floodFilled = false;
  bool canvasMoved = false;
  static bool moveUndoSaved = false;
  // Check if enter or delete is currently being held (for drawing while moving)
  bool enterHeld = input.enterHeld;
  bool deleteHeld = input.deleteHeld;

  if (input.actionPressed) {
    clearCanvas();
    canvasCleared = true;
    LED_CANVAS_UPDATED();  // Update LED matrix
  }

  // Check if M key is held (for canvas move)
  bool mHeld = input.mHeld;
  bool moveModeChanged = (mHeld != editorState.moveModeActive);
  editorState.moveModeActive = mHeld;
  if (!mHeld) moveUndoSaved = false;

#if ENABLE_BLUETOOTH
  // Merge BT input with keyboard input
  // btSpace also acts as draw (BT only feature)
  enterHeld = enterHeld || btEnter || btSpace;
  deleteHeld = deleteHeld || btBackspace;

  // Check for BT Fn modifier (Alt key)
  bool fnHeld = input.fnHeld || btFnHeld;

  // Process BT arrow keys - detect new presses and set up for key repeat
  static bool btPrevUp = false, btPrevDown = false, btPrevLeft = false, btPrevRight = false;

  // Check for new BT arrow presses
  if (btArrowUp && !btPrevUp) {
    lastKey = ';';  // Use same codes as built-in keyboard
    lastKeyTime = millis();
    keyRepeating = false;
    if (editorState.cursorY > 0) { editorState.cursorY--; moved = true; }
  }
  if (btArrowDown && !btPrevDown) {
    lastKey = '.';
    lastKeyTime = millis();
    keyRepeating = false;
    if (editorState.cursorY < editorState.gridSize - 1) { editorState.cursorY++; moved = true; }
  }
  if (btArrowLeft && !btPrevLeft) {
    lastKey = ',';
    lastKeyTime = millis();
    keyRepeating = false;
    if (editorState.cursorX > 0) { editorState.cursorX--; moved = true; }
  }
  if (btArrowRight && !btPrevRight) {
    lastKey = '/';
    lastKeyTime = millis();
    keyRepeating = false;
    if (editorState.cursorX < editorState.gridSize - 1) { editorState.cursorX++; moved = true; }
  }

  btPrevUp = btArrowUp; btPrevDown = btArrowDown;
  btPrevLeft = btArrowLeft; btPrevRight = btArrowRight;

  // Process BT Enter/Space/Backspace for pixel operations
  // Space on BT keyboard also draws (BT-only feature)
  if ((btEnter || btSpace) && !input.enterHeld) {
    saveUndo();
    editorState.canvas[editorState.cursorY][editorState.cursorX] = editorState.selectedColor;
    pixelPlaced = true;
    LED_CANVAS_UPDATED();
  }
  if (btBackspace && !input.deleteHeld) {
    saveUndo();
    editorState.canvas[editorState.cursorY][editorState.cursorX] = 0;
    pixelPlaced = true;
    LED_CANVAS_UPDATED();
  }

  // Process BT character queue
  char btChar;
  while (btQueuePop(btChar)) {
    // Number keys 1-8 select colors
    if (btChar >= '1' && btChar <= '8') {
      uint8_t baseColor = btChar - '0';
      uint8_t newColor = fnHeld ? (baseColor + 8) : baseColor;
      if (newColor <= documentState.sketch.paletteSize && editorState.selectedColor != newColor) {
        editorState.selectedColor = newColor;
        colorChanged = true;
        char colorMsg[20];
        snprintf(colorMsg, sizeof(colorMsg), StatusMsg::COLOR_FMT, editorState.selectedColor);
        setStatusMessage(colorMsg);
      }
    }
    // C key - Cycle color
    else if (btChar == 'c' || btChar == 'C') {
      editorState.selectedColor++;
      if (editorState.selectedColor > documentState.sketch.paletteSize) editorState.selectedColor = 1;
      colorChanged = true;
      char colorMsg[20];
      snprintf(colorMsg, sizeof(colorMsg), StatusMsg::COLOR_FMT, editorState.selectedColor);
      setStatusMessage(colorMsg);
    }
    // Z key - Undo
    else if (btChar == 'z' || btChar == 'Z') {
      if (fnHeld) {
        restoreRedo();
      } else {
        restoreUndo();
      }
      undoPerformed = true;
    }
    // G key - Toggle grid
    else if (btChar == 'g' || btChar == 'G') {
      toggleGridSize();
      gridToggled = true;
    }
    // R key - Toggle rulers
    else if (btChar == 'r' || btChar == 'R') {
      editorState.rulersVisible = !editorState.rulersVisible;
      rulersToggled = true;
      setStatusMessage(editorState.rulersVisible ? "Rulers: On" : "Rulers: Off");
    }
    // O key - Memory view
    else if (btChar == 'o' || btChar == 'O') {
      enterMemoryView();
      delay(200);
      return;
    }
    // P key - Palette view
    else if (btChar == 'p' || btChar == 'P') {
      enterPaletteView();
      delay(200);
      return;
    }
    // H key - Help view
    else if (btChar == 'h' || btChar == 'H') {
      enterHelpView();
      delay(200);
      return;
    }
    // S key - Save sketch (or Alt+S to save as new)
    else if (btChar == 's' || btChar == 'S') {
      // Copy canvas to active sketch before saving
      for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
          documentState.sketch.pixels[y][x] = editorState.canvas[y][x];
        }
      }
      documentState.sketch.gridSize = editorState.gridSize;
      if (fnHeld) {
        saveActiveSketchAsNew();
      } else {
        saveActiveSketchToSD();
      }
    }
    // V key - Preview view
    else if (btChar == 'v' || btChar == 'V') {
      enterPreviewView();
      delay(200);
      return;
    }
    // T key - Settings view
    else if (btChar == 't' || btChar == 'T') {
      enterSettingsView();
      delay(200);
      return;
    }
    // F key - Flood fill
    else if (btChar == 'f' || btChar == 'F') {
      saveUndo();
      floodFill(editorState.cursorX, editorState.cursorY, editorState.selectedColor);
      floodFilled = true;
      LED_CANVAS_UPDATED();
      setStatusMessage(StatusMsg::FILL);
    }
    // B key (with Fn/Alt) - Charging mode
    else if ((btChar == 'b' || btChar == 'B') && fnHeld) {
      enterChargingMode();
      delay(200);
      return;
    }
  }

  // Clear BT arrow state after processing (they're edge-triggered from HID reports)
  // Note: We don't clear here because HID reports continuously update the state
#else
  bool fnHeld = input.fnHeld;
#endif

  const bool drawPressed = enterHeld;
  const bool erasePressed = deleteHeld;
  const bool fillPressed =
      input.fHeld
#if ENABLE_BLUETOOTH
      || btFill
#endif
      ;
  const bool toolStateChanged =
      drawPressed != editorState.drawPressed ||
      erasePressed != editorState.erasePressed ||
      fillPressed != editorState.fillPressed;
  editorState.drawPressed = drawPressed;
  editorState.erasePressed = erasePressed;
  editorState.fillPressed = fillPressed;

  if (input.enterPressed) {
    saveUndo();
    editorState.canvas[editorState.cursorY][editorState.cursorX] = editorState.selectedColor;
    pixelPlaced = true;
    LED_CANVAS_UPDATED();
  } else if (input.deletePressed) {
    saveUndo();
    editorState.canvas[editorState.cursorY][editorState.cursorX] = 0;
    pixelPlaced = true;
    LED_CANVAS_UPDATED();
  }

  uint8_t requestedColor = 0;
  switch (input.event) {
    case bitmap16::InputEvent::Number1: requestedColor = 1; break;
    case bitmap16::InputEvent::Number2: requestedColor = 2; break;
    case bitmap16::InputEvent::Number3: requestedColor = 3; break;
    case bitmap16::InputEvent::Number4: requestedColor = 4; break;
    case bitmap16::InputEvent::Number5: requestedColor = 5; break;
    case bitmap16::InputEvent::Number6: requestedColor = 6; break;
    case bitmap16::InputEvent::Number7: requestedColor = 7; break;
    case bitmap16::InputEvent::Number8: requestedColor = 8; break;
    default: break;
  }

  if (requestedColor != 0) {
    const uint8_t newColor =
        fnHeld ? requestedColor + 8 : requestedColor;
    if (newColor <= documentState.sketch.paletteSize && editorState.selectedColor != newColor) {
      editorState.selectedColor = newColor;
      colorChanged = true;
      char colorMsg[20];
      snprintf(
          colorMsg,
          sizeof(colorMsg),
          StatusMsg::COLOR_FMT,
          editorState.selectedColor);
      setStatusMessage(colorMsg);
    }
  }

  const bool characterEvent =
      input.event == bitmap16::InputEvent::Character;
  const char command = characterEvent ? input.character : '\0';

  if (command == 'c' || command == 'C') {
    ++editorState.selectedColor;
    if (editorState.selectedColor > documentState.sketch.paletteSize) {
      editorState.selectedColor = 1;
    }
    colorChanged = true;
    char colorMsg[20];
    snprintf(
        colorMsg,
        sizeof(colorMsg),
        StatusMsg::COLOR_FMT,
        editorState.selectedColor);
    setStatusMessage(colorMsg);
  } else if (command == 'z' || command == 'Z') {
    if (fnHeld) {
      restoreRedo();
    } else {
      restoreUndo();
    }
    undoPerformed = true;
  } else if (command == 'g' || command == 'G') {
    toggleGridSize();
    gridToggled = true;
  } else if (command == 'r' || command == 'R') {
    editorState.rulersVisible = !editorState.rulersVisible;
    rulersToggled = true;
    setStatusMessage(editorState.rulersVisible ? "Rulers: On" : "Rulers: Off");
  } else if (command == 'o' || command == 'O') {
    enterMemoryView();
  } else if (command == 's' || command == 'S') {
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        documentState.sketch.pixels[y][x] = editorState.canvas[y][x];
      }
    }
    documentState.sketch.gridSize = editorState.gridSize;
    if (fnHeld) {
      saveActiveSketchAsNew();
    } else {
      saveActiveSketchToSD();
    }
  } else if (command == 'f' || command == 'F') {
    saveUndo();
    floodFill(editorState.cursorX, editorState.cursorY, editorState.selectedColor);
    floodFilled = true;
    LED_CANVAS_UPDATED();
    setStatusMessage(StatusMsg::FILL);
  } else if (command == 'h' || command == 'H') {
    enterHelpView();
  } else if (command == 't' || command == 'T') {
    enterSettingsView();
  } else if (command == 'v' || command == 'V') {
    enterPreviewView();
  } else if (command == 'x' || command == 'X') {
    exportCanvasToPNG(!fnHeld);
#if ENABLE_SCREENSHOTS
  } else if (command == 'y' || command == 'Y') {
    takeScreenshot();
#endif
  } else if (command == 'p' || command == 'P') {
    enterPaletteView();
  } else if (fnHeld && input.bHeld) {
    enterChargingMode();
    return;
  } else if (input.bHeld &&
             (input.event == bitmap16::InputEvent::Plus ||
              input.event == bitmap16::InputEvent::Minus)) {
    const int BRIGHTNESS_STEP = 10;
    const int MIN_BRIGHTNESS = 10;
    const int MAX_BRIGHTNESS = 100;
    if (input.event == bitmap16::InputEvent::Plus) {
      displayBrightness =
          min(MAX_BRIGHTNESS, displayBrightness + BRIGHTNESS_STEP);
    } else {
      displayBrightness =
          max(MIN_BRIGHTNESS, displayBrightness - BRIGHTNESS_STEP);
    }

    Display::setBrightness(displayBrightness);
    PreferenceStore::writeUInt8("brightness", displayBrightness);

    char brightnessMsg[20];
    snprintf(
        brightnessMsg,
        sizeof(brightnessMsg),
        "BRIGHT: %d%%",
        displayBrightness);
    setStatusMessage(brightnessMsg);
  }

#if ENABLE_LED_MATRIX
  if (input.lHeld && input.enterPressed) {
    toggleLEDMatrix();
    char ledMsg[30];
    snprintf(
        ledMsg,
        sizeof(ledMsg),
        "LED: %s",
        LEDMatrix::isEnabled() ? "ON" : "OFF");
    setStatusMessage(ledMsg);
  } else if (input.lHeld &&
             (input.event == bitmap16::InputEvent::Plus ||
              input.event == bitmap16::InputEvent::Minus)) {
    adjustLEDBrightness(
        input.event == bitmap16::InputEvent::Minus ? -1 : 1);
    char ledBrightMsg[30];
    snprintf(
        ledBrightMsg,
        sizeof(ledBrightMsg),
        "LED: %d%%",
        ledBrightness);
    setStatusMessage(ledBrightMsg);
  }
#endif

  const bool directionEvent =
      input.event == bitmap16::InputEvent::Up ||
      input.event == bitmap16::InputEvent::Down ||
      input.event == bitmap16::InputEvent::Left ||
      input.event == bitmap16::InputEvent::Right;
  if (directionEvent) {
    if (mHeld) {
      if (!moveUndoSaved) {
        saveUndo();
        moveUndoSaved = true;
      }
      if (input.event == bitmap16::InputEvent::Up) {
        shiftCanvas(0, -1);
      } else if (input.event == bitmap16::InputEvent::Down) {
        shiftCanvas(0, 1);
      } else if (input.event == bitmap16::InputEvent::Left) {
        shiftCanvas(-1, 0);
      } else {
        shiftCanvas(1, 0);
      }
      canvasMoved = true;
      LED_CANVAS_UPDATED();
      setStatusMessage(StatusMsg::MOVE);
    } else {
      if (input.event == bitmap16::InputEvent::Up && editorState.cursorY > 0) {
        --editorState.cursorY;
        moved = true;
      } else if (input.event == bitmap16::InputEvent::Down &&
                 editorState.cursorY < editorState.gridSize - 1) {
        ++editorState.cursorY;
        moved = true;
      } else if (input.event == bitmap16::InputEvent::Left && editorState.cursorX > 0) {
        --editorState.cursorX;
        moved = true;
      } else if (input.event == bitmap16::InputEvent::Right &&
                 editorState.cursorX < editorState.gridSize - 1) {
        ++editorState.cursorX;
        moved = true;
      }

      if (moved && enterHeld) {
        editorState.canvas[editorState.cursorY][editorState.cursorX] = editorState.selectedColor;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();
      } else if (moved && deleteHeld) {
        editorState.canvas[editorState.cursorY][editorState.cursorX] = 0;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();
      }
    }
  }

#if ENABLE_BLUETOOTH
  bool btArrowHeld = false;
  char currentBtArrow = 0;
  if (btArrowUp) { btArrowHeld = true; currentBtArrow = ';'; }
  else if (btArrowDown) { btArrowHeld = true; currentBtArrow = '.'; }
  else if (btArrowLeft) { btArrowHeld = true; currentBtArrow = ','; }
  else if (btArrowRight) { btArrowHeld = true; currentBtArrow = '/'; }

  if (btArrowHeld && currentBtArrow == lastKey) {
    const unsigned long currentTime = millis();
    const unsigned long threshold =
        keyRepeating ? keyRepeatRate : keyRepeatDelay;
    if (currentTime - lastKeyTime >= threshold) {
      keyRepeating = true;
      lastKeyTime = currentTime;
      if (mHeld) {
        if (!moveUndoSaved) {
          saveUndo();
          moveUndoSaved = true;
        }
        if (currentBtArrow == ';') shiftCanvas(0, -1);
        else if (currentBtArrow == '.') shiftCanvas(0, 1);
        else if (currentBtArrow == ',') shiftCanvas(-1, 0);
        else shiftCanvas(1, 0);
        canvasMoved = true;
        LED_CANVAS_UPDATED();
      } else {
        if (currentBtArrow == ';' && editorState.cursorY > 0) {
          --editorState.cursorY;
          moved = true;
        } else if (currentBtArrow == '.' &&
                   editorState.cursorY < editorState.gridSize - 1) {
          ++editorState.cursorY;
          moved = true;
        } else if (currentBtArrow == ',' && editorState.cursorX > 0) {
          --editorState.cursorX;
          moved = true;
        } else if (currentBtArrow == '/' &&
                   editorState.cursorX < editorState.gridSize - 1) {
          ++editorState.cursorX;
          moved = true;
        }
        if (moved && enterHeld) {
          editorState.canvas[editorState.cursorY][editorState.cursorX] = editorState.selectedColor;
          pixelPlaced = true;
          LED_CANVAS_UPDATED();
        } else if (moved && deleteHeld) {
          editorState.canvas[editorState.cursorY][editorState.cursorX] = 0;
          pixelPlaced = true;
          LED_CANVAS_UPDATED();
        }
      }
    }
  } else if (!btArrowHeld) {
    lastKey = 0;
    keyRepeating = false;
  }
#endif

  // Update LED matrix when cursor moves (to highlight current position)
  if (moved) {
    LED_CANVAS_UPDATED();
  }

  // Update status expiry before the shared full-frame renderer runs.
  drawStatusMessage();
  drawBatteryIndicator();

  const bool canvasViewChanged =
      moved || pixelPlaced || colorChanged || canvasCleared ||
      undoPerformed || gridToggled || rulersToggled || themeToggled ||
      floodFilled || canvasMoved || moveModeChanged ||
      toolStateChanged ||
      statusMessage[0] != '\0' || statusMessageJustCleared;
  if (canvasViewChanged) {
    drawSharedCanvasView();
    statusMessageJustCleared = false;
  }

  // Periodic heap monitoring (every 60 seconds)
  // Helps catch memory leaks during development and extended use
  unsigned long currentTime = millis();
  if (currentTime - lastHeapCheckTime >= HEAP_CHECK_INTERVAL) {
    lastHeapCheckTime = currentTime;

    int freeHeap = ESP.getFreeHeap();

    // Show warning if memory is getting low
    if (freeHeap < HEAP_WARNING_THRESHOLD) {
      char warningMsg[32];
      snprintf(warningMsg, sizeof(warningMsg), StatusMsg::LOW_MEMORY_FMT, freeHeap / 1024);
      setStatusMessage(warningMsg);
    }
  }

#if ENABLE_LED_MATRIX
  // Update LED matrix if canvas has changed
  if (canvasNeedsUpdate) {
    updateLEDMatrix();
    canvasNeedsUpdate = false;  // Clear flag
  }
#endif // ENABLE_LED_MATRIX

  // Small delay to prevent the loop from running too fast
  delay(10);
}
