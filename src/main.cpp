/**
 * BitMap16 DX - v0.5.0
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
 * - S to save current canvas as snapshot
 * - Fn+S to save as new sketch
 * - O to open Memory View (browse/load saved snapshots)
 * - I to open Controls/Help screen
 * - V to view canvas (128×128, centered)
 *   - In view mode: 1=black bg, 2=white bg, 3=gray bg
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
#include <SD.h>
#include <SPI.h>
#include <PNGENC.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include "boot_image.h"

// Preferences for persistent storage across reboots
Preferences preferences;

// ============================================================================
// FEATURE FLAGS
// ============================================================================

// Enable screenshot feature (Y key) - disable for release builds
#define ENABLE_SCREENSHOTS 1  // Set to 0 to disable screenshots in release

// Enable external 8×8 WS2812 LED matrix support
// Set to 0 to disable LED matrix features and save memory (~9KB flash, 880 bytes RAM)
#define ENABLE_LED_MATRIX 1  // Set to 0 to disable

// Macro for LED matrix canvas updates (no-op when feature disabled)
#if ENABLE_LED_MATRIX
  #define LED_CANVAS_UPDATED() canvasNeedsUpdate = true
#else
  #define LED_CANVAS_UPDATED() ((void)0)
#endif

// ============================================================================
// CONFIGURATION
// ============================================================================


// File format version for sketch files
// Version 1: gridSize (1B) + paletteSize (1B) + palette (32B) + pixels (256B) = 290 bytes
// Version 2: formatVersion (1B) + gridSize (1B) + paletteSize (1B) + palette (32B) + pixels (256B) = 291 bytes
const uint8_t SKETCH_FORMAT_VERSION = 2;
const int SKETCH_FILE_SIZE_V1 = 290;  // Legacy format without version byte
const int SKETCH_FILE_SIZE_V2 = 291;  // Current format with version byte

// Canvas size in logical pixels
// The canvas is always 16×16 to support both modes
const int MAX_currentGridSize = 16;

// Current grid size (can be 8 or 16, toggled with 'G' key)
int currentGridSize = 8;

// Cell size changes based on grid mode:
// - 8×8 mode: 16px cells (128×128 total)
// - 16×16 mode: 8px cells (128×128 total)
int currentCellSize = 16;

// Palette column configuration (right side)
const int PALETTE_SWATCH_SIZE = 16;   // Each color swatch is 16×16 pixels
const int PALETTE_WIDTH = 32;         // Width of the palette column (2 columns × 16px)
const int PALETTE_MARGIN = 5;         // Margin from right edge
const int PALETTE_X = 240 - PALETTE_WIDTH - PALETTE_MARGIN;  // Position: 5px from right edge

// Where to draw the grid on screen
// Cardputer display is 240×135 pixels
// Grid is always 128×128 pixels on screen (regardless of 8×8 or 16×16 mode):
//   - 8×8 mode: 8 cells × 16px = 128px
//   - 16×16 mode: 16 cells × 8px = 128px
// Center the canvas horizontally: (240 - 128) / 2 = 56px
const int GRID_X = 56;   // Start X position (horizontally centered)
const int GRID_Y = 4;    // Start Y position (centered vertically)

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

const int CARTRIDGE_WIDTH = 80;
const int CARTRIDGE_HEIGHT = 92;

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
  RGB565(0xD3, 0xD3, 0xDD),  // centerLine (same as background)
  TFT_BLACK,                 // iconDark #000000
  TFT_WHITE                  // iconLight #ffffff
};

// Dark theme definition
const ThemeColors THEME_DARK = {
  0x2105,                    // background #202226
  RGB565(0x9E, 0x9E, 0x9E),  // cellDark #9c9c9c
  RGB565(0xBD, 0xBA, 0xBA),  // cellLight #bdbaba
  RGB565(0x15, 0x17, 0x1A),  // shadow #15171A
  TFT_WHITE,                 // text #ffffff
  0x2105,                    // centerLine (same as background)
  TFT_BLACK,                 // iconDark #000000
  RGB565(0xEE, 0xEF, 0xF4)   // iconLight #EEEFF4
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

// Current cursor position (in grid coordinates, not screen pixels)
int cursorX = 0;
int cursorY = 0;

// Previous cursor screen position (to clear the old cursor icon)
int lastCursorScreenX = -1;
int lastCursorScreenY = -1;

// Key repeat timing
unsigned long lastKeyTime = 0;        // When the last key action happened
unsigned long keyRepeatDelay = 300;   // Initial delay before repeat starts (ms)
unsigned long keyRepeatRate = 100;    // Time between repeats (ms)
bool keyRepeating = false;            // Whether we're in repeat mode
char lastKey = 0;                     // Track which arrow key is held

// Canvas data - stores color indices for each pixel
// 0 = empty/transparent (show checkerboard)
// 1-16 = color indices into the PALETTE array
// Canvas is always 16×16 to support both grid modes
uint8_t canvas[16][16] = {0};

// Currently selected color (1-16)
uint8_t selectedColor = 1;  // Start with color 1 (black)

// Center rulers/guides visibility
bool rulersVisible = false;  // Toggle with R key

// Display brightness level (stored as percentage: 10-100%)
// Converted to hardware range (0-255) when setting display
uint8_t displayBrightness = 80;  // Start at 80% brightness

#if ENABLE_LED_MATRIX
// ============================================================================
// LED MATRIX CONFIGURATION (8×8 WS2812 RGB LEDs)
// ============================================================================

#include <FastLED.h>

// 8×8 WS2812E RGB LED matrix (64 LEDs)
// Mirrors the 8×8 canvas in real-time when enabled
// Connected to Port A: Yellow wire (G2) = GPIO2
#define LED_PIN 2                     // GPIO2 (Port A - Yellow wire)
#define NUM_LEDS 64                   // 8×8 matrix
#define DEFAULT_LED_BRIGHTNESS 10     // 10% brightness (132mA)
#define MIN_LED_BRIGHTNESS 5          // Minimum 5%
#define MAX_LED_BRIGHTNESS 20         // Maximum 20% (for battery safety)

CRGB leds[NUM_LEDS];                  // LED array for FastLED
bool ledMatrixEnabled = false;        // User must explicitly enable
uint8_t ledBrightness = DEFAULT_LED_BRIGHTNESS;  // 5-20%
bool canvasNeedsUpdate = false;       // Flag to trigger LED update
#endif // ENABLE_LED_MATRIX

// Undo state - stores a single previous canvas state
uint8_t undoCanvas[16][16] = {0};
bool undoAvailable = false;

// Undo palette state - stores palette info for sketch undo operations
uint8_t undoPaletteSize = 0;
uint16_t undoPaletteColors[16] = {0};
uint8_t undoGridSize = 0;

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

// Active sketch in memory (only one sketch loaded at a time)
Sketch activeSketch;
bool activeSketchIsNew = true;           // True if never saved
String activeSketchFilename = "";        // e.g., "sketch_1737849600.dat"

// Dynamic sketch list for memory view
struct SketchInfo {
  String filename;                       // e.g., "sketch_1737849600.dat"
  unsigned long timestamp;               // Unix timestamp from filename
  Sketch sketchData;                     // Cached sketch data (loaded once when entering memory view)
  bool dataLoaded;                       // Whether sketchData is valid
};

std::vector<SketchInfo> sketchList;      // Populated when entering memory view

// Memory View state
bool inMemoryView = false;
int memoryViewCursor = 0;  // Which item is selected (0 = "+", 1+ = sketches)
int memoryViewScrollOffset = 0;  // Target scroll position (pixels scrolled left)
float memoryViewScrollPos = 0.0f;  // Actual animated scroll position for smooth transitions
const float MEMORY_SCROLL_SPEED = 0.35f;  // Animation speed (0.0-1.0, higher = faster)
unsigned long lastMemoryAnimTime = 0;  // For frame rate limiting
const int MEMORY_ANIM_FRAME_MS = 16;  // Milliseconds between animation frames (16ms = 60fps - now we can afford it with caching!)

// Memory View cursor animation (diagonal breathing effect)
float memoryCursorAnimPhase = 0.0f;  // Animation phase (0.0 to 1.0, loops)
const float MEMORY_CURSOR_ANIM_SPEED = 0.010f;  // How fast the animation cycles (lower = slower, more calm)
const int MEMORY_CURSOR_ANIM_DISTANCE = 6;  // Max pixels to move diagonally (more pixels = smoother animation)

// Hint screen state
bool inHelpView = false;
bool helpViewFromMemoryView = false;  // Track if hint screen was opened from memory view

// View mode state
bool inPreviewView = false;
uint8_t previewViewBackground = 0;  // 0=black, 1=white, 2=light gray, 3=dark gray

// Palette menu state
bool inPaletteView = false;
bool paletteCanvasAvailable = false;  // Track if canvas allocation succeeded
int paletteViewCursor = 0;  // Currently selected palette (0 to totalPaletteCount-1)
float paletteViewScrollPos = 0.0f;  // Animated scroll position for smooth transitions
const float PALETTE_SCROLL_SPEED = 0.25f;  // Animation speed (0.0-1.0, higher = faster)
unsigned long lastPaletteAnimTime = 0;  // For frame rate limiting

// Heap monitoring state
unsigned long lastHeapCheckTime = 0;
const unsigned long HEAP_CHECK_INTERVAL = 60000;  // Check every 60 seconds
const int HEAP_WARNING_THRESHOLD = 50000;  // Warn if free heap drops below 50KB
const int PALETTE_ANIM_FRAME_MS = 16;  // Milliseconds between animation frames (16ms = 60fps)

// Palette insertion animation state
bool paletteInsertionAnimating = false;
float paletteInsertionProgress = 0.0f;  // 0.0 = start position, 1.0 = inserted
float paletteInsertionFrozenScrollPos = 0.0f;  // Frozen scroll position for side cartridges during animation
const float PALETTE_INSERT_SPEED = 0.12f;  // Animation speed (with impact feel)
const int PALETTE_INSERT_DISTANCE = 36;  // Distance to move down (top goes from Y=20 to Y=56)

// Canvas for smooth palette menu rendering (eliminates screen tearing)
M5Canvas paletteCanvas(&M5Cardputer.Display);

// Canvas for smooth memory view rendering (eliminates screen tearing)
M5Canvas memoryCanvas(&M5Cardputer.Display);

// Battery display
int lastBatteryPercent = -1;  // Track last drawn battery % to avoid unnecessary redraws
unsigned long lastBatteryCheckTime = 0;  // Track when we last checked battery
const unsigned long BATTERY_CHECK_INTERVAL = 30000;  // Check battery every 30 seconds
bool batteryFirstCheck = true;  // Flag to force first battery check

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

// SD card pins (same for both M5Cardputer and M5Cardputer ADV)
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

// ============================================================================
// ICON DRAWING FUNCTIONS
// ============================================================================

/**
 * Draw an icon - supports both 1-bit and 2-bit indexed formats
 *
 * @param x X position
 * @param y Y position
 * @param bitmap Icon data
 * @param w Width in pixels
 * @param h Height in pixels
 * @param indexed If true, uses 2-bit indexed format (0=transparent, 1=black, 2=white)
 *                If false, uses 1-bit format (1=black, 0=transparent)
 */
void drawIcon(int x, int y, const unsigned char* bitmap, int w, int h, bool indexed = false) {
  if (indexed) {
    // 2-bit indexed format: 4 pixels per byte
    // 0 = transparent, 1 = dark, 2 = light
    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        int pixelIndex = row * w + col;
        int byteIndex = pixelIndex / 4;
        int bitShift = (3 - (pixelIndex % 4)) * 2;  // 6, 4, 2, 0

        uint8_t byte = pgm_read_byte(&bitmap[byteIndex]);
        uint8_t value = (byte >> bitShift) & 0x03;

        if (value == 1) {
          M5Cardputer.Display.drawPixel(x + col, y + row, currentTheme->iconDark);
        } else if (value == 2) {
          M5Cardputer.Display.drawPixel(x + col, y + row, currentTheme->iconLight);
        }
        // value == 0 is transparent, skip
      }
    }
  } else {
    // 1-bit format: 1 = dark, 0 = transparent
    int byteWidth = (w + 7) / 8;
    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        if (col % 8 == 0) {
          uint8_t byte = pgm_read_byte(&bitmap[row * byteWidth + col / 8]);
          if (byte & (0x80 >> (col % 8))) {
            M5Cardputer.Display.drawPixel(x + col, y + row, currentTheme->iconDark);
          }
        }
      }
    }
  }
}


/**
 * Set a status message to display temporarily
 */
void setStatusMessage(const char* message) {
  strncpy(statusMessage, message, sizeof(statusMessage) - 1);
  statusMessage[sizeof(statusMessage) - 1] = '\0';  // Ensure null termination
  statusMessageTime = millis();
}

/**
 * Check if the B key is currently being held down
 * Used for brightness control (B + Plus/Minus)
 */
bool isBKeyHeld(Keyboard_Class::KeysState& status) {
  for (auto i : status.word) {
    if (i == 'b' || i == 'B') {
      return true;
    }
  }
  return false;
}

#if ENABLE_LED_MATRIX
// Helper function to check if 'L' key is in the word buffer
bool isLKeyHeld(Keyboard_Class::KeysState& status) {
  for (auto i : status.word) {
    if (i == 'l' || i == 'L') {
      return true;
    }
  }
  return false;
}
#endif // ENABLE_LED_MATRIX

// ============================================================================
// ANIMATION HELPER FUNCTIONS
// ============================================================================

/**
 * Ease-in-out function (cubic easing)
 * Takes a value from 0.0 to 1.0 and returns eased value from 0.0 to 1.0
 * The output smoothly accelerates at the start and decelerates at the end
 */
float easeInOutCubic(float t) {
  if (t < 0.5f) {
    return 4.0f * t * t * t;  // Ease in (accelerate)
  } else {
    float f = (2.0f * t - 2.0f);
    return 0.5f * f * f * f + 1.0f;  // Ease out (decelerate)
  }
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
      activeSketch.pixels[y][x] = 0;
    }
  }

  // Default to 16×16 grid with 16-color palette
  activeSketch.gridSize = 16;
  activeSketch.paletteSize = 16;

  // Copy the default palette (first palette in catalog: SWEETIE-16)
  for (int i = 0; i < 16; i++) {
    activeSketch.paletteColors[i] = pgm_read_word(&allPalettes[0][i]);
  }

  activeSketch.isEmpty = true;
  activeSketchIsNew = true;
  activeSketchFilename = "";
}

/**
 * Collapse a pixel index to valid range for smaller palettes
 * Rules:
 * - 4-color: collapsedIndex = ((index - 1) % 4) + 1 (Examples: 5→1, 6→2, ... 16→4)
 * - 8-color: collapsedIndex = ((index - 1) % 8) + 1 (Examples: 9→1, 10→2, ... 16→8)
 */
uint8_t collapseIndex(uint8_t index, uint8_t paletteSize) {
  if (index == 0) return 0;  // Transparent stays transparent
  if (index <= paletteSize) return index;  // Already valid
  return ((index - 1) % paletteSize) + 1;
}

/**
 * Get the rendered color for a pixel index in the active sketch
 * Handles color collapse for smaller palettes (4 or 8 colors)
 */
uint16_t getActiveSketchPixelColor(uint8_t pixelIndex) {
  if (pixelIndex == 0) {
    // Transparent - return 0 (caller should render checkerboard)
    return 0;
  }

  // If palette is smaller than 16 colors and index exceeds palette size, collapse it
  if (activeSketch.paletteSize < 16 && pixelIndex > activeSketch.paletteSize) {
    pixelIndex = collapseIndex(pixelIndex, activeSketch.paletteSize);
  }

  // Return the color (array is 0-indexed, so pixelIndex 1 → array[0])
  // paletteColors[0..15] stores colors for pixel indices 1..16
  return activeSketch.paletteColors[pixelIndex - 1];
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
  // Only try to initialize once
  if (sdCardInitialized) {
    return sdCardAvailable;
  }

  // Mark as initialized before attempting (prevent retry loops)
  sdCardInitialized = true;

  // Small delay to let SD card stabilize after power-on
  // This helps with timing-sensitive cards
  delay(100);

  // Initialize SPI with explicit pins (same for both Cardputer models)
  // Pins: CLK=40, MOSI=14, MISO=39, CS=12
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

  // Try multiple times with delays between attempts
  // Some SD cards need more time to initialize
  for (int attempt = 0; attempt < 3; attempt++) {
    // Try to initialize SD card with explicit CS pin at 25MHz
    if (SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
      // Card initialized - verify card type
      uint8_t cardType = SD.cardType();

      if (cardType != CARD_NONE) {
        // Final verification - check card size
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);  // Convert to MB

        // Create base directory structure if it doesn't exist
        if (!SD.exists("/bitmap16dx")) {
          SD.mkdir("/bitmap16dx");
        }
        if (!SD.exists("/bitmap16dx/sketches")) {
          SD.mkdir("/bitmap16dx/sketches");
        }
        if (!SD.exists("/bitmap16dx/exports")) {
          SD.mkdir("/bitmap16dx/exports");
        }
#if ENABLE_SCREENSHOTS
        if (!SD.exists("/bitmap16dx/screenshots")) {
          SD.mkdir("/bitmap16dx/screenshots");
        }
#endif
        if (!SD.exists("/bitmap16dx/palettes")) {
          SD.mkdir("/bitmap16dx/palettes");
        }

        // SD card is ready
        sdCardAvailable = true;
        return true;
      }
    }

    // Wait before retry (except on last attempt)
    if (attempt < 2) {
      delay(100);
    }
  }

  // All attempts failed
  sdCardAvailable = false;
  return false;
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
void loadSketchListFromSD() {
  sketchList.clear();

  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  // Quick card check
  if (SD.cardType() == CARD_NONE) {
    sdCardAvailable = false;
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  // Open /bitmap16dx/sketches directory
  File root = SD.open("/bitmap16dx/sketches");
  if (!root || !root.isDirectory()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return;
  }

  // Iterate through all files
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = String(file.name());

      // ESP32 SD library might return full path or just filename
      // Extract just the filename if it contains a path
      int lastSlash = filename.lastIndexOf('/');
      if (lastSlash >= 0) {
        filename = filename.substring(lastSlash + 1);
      }

      // Check if it matches "sketch_XXXXXXXXXX.dat" pattern
      if (filename.startsWith("sketch_") && filename.endsWith(".dat")) {
        // Extract timestamp from filename
        // Format: sketch_1234567890.dat
        //         0123456789...
        int underscorePos = filename.indexOf('_');
        int dotPos = filename.lastIndexOf('.');

        if (underscorePos >= 0 && dotPos > underscorePos) {
          String timestampStr = filename.substring(underscorePos + 1, dotPos);
          unsigned long timestamp = timestampStr.toInt();

          // Verify file size is correct (290 or 291 bytes) and timestamp is valid
          if ((file.size() == SKETCH_FILE_SIZE_V1 || file.size() == SKETCH_FILE_SIZE_V2) && timestamp > 0) {
            SketchInfo info;
            info.filename = filename;
            info.timestamp = timestamp;
            info.dataLoaded = false;  // Will load data on demand
            sketchList.push_back(info);
          }
        }
      }
    }

    file = root.openNextFile();
  }

  root.close();

  // Sort by timestamp (newest first)
  std::sort(sketchList.begin(), sketchList.end(),
            [](const SketchInfo& a, const SketchInfo& b) {
              return a.timestamp > b.timestamp;
            });
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

  // Quick card check
  if (SD.cardType() == CARD_NONE) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    sdCardAvailable = false;
    return false;
  }

  // Create directories if needed
  if (!SD.exists("/bitmap16dx/sketches")) {
    if (!SD.mkdir("/bitmap16dx/sketches")) {
      setStatusMessage(StatusMsg::SD_NOT_READY);
      sdCardAvailable = false;
      return false;
    }
  }

  // Generate filename (use existing if saving in place, or new timestamp if new)
  String fullPath;
  if (activeSketchFilename.length() > 0 && !activeSketchIsNew) {
    // Save to existing file
    fullPath = "/bitmap16dx/sketches/" + activeSketchFilename;
  } else {
    // Create new file with incrementing counter (persists across reboots)
    preferences.begin("bitmap16dx", false);
    unsigned long counter = preferences.getULong("sketchCounter", 0);

    // If counter is 0 (first time or after NVS reset), scan existing files to find highest number
    if (counter == 0 && SD.exists("/bitmap16dx/sketches")) {
      File root = SD.open("/bitmap16dx/sketches");
      if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
          if (!file.isDirectory()) {
            String filename = String(file.name());
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
              filename = filename.substring(lastSlash + 1);
            }

            // Extract number from "sketch_NNNN.dat"
            if (filename.startsWith("sketch_") && filename.endsWith(".dat")) {
              int underscorePos = filename.indexOf('_');
              int dotPos = filename.lastIndexOf('.');
              if (underscorePos >= 0 && dotPos > underscorePos) {
                String numStr = filename.substring(underscorePos + 1, dotPos);
                unsigned long num = numStr.toInt();
                if (num > counter) {
                  counter = num;
                }
              }
            }
          }
          file = root.openNextFile();
        }
        root.close();
      }
    }

    counter++;
    preferences.putULong("sketchCounter", counter);
    preferences.end();

    fullPath = "/bitmap16dx/sketches/sketch_" + String(counter) + ".dat";
    activeSketchFilename = "sketch_" + String(counter) + ".dat";

    activeSketchIsNew = false;
  }

  // Delete existing file if it exists (FILE_WRITE appends, we want to overwrite)
  if (SD.exists(fullPath.c_str())) {
    SD.remove(fullPath.c_str());
  }

  // Open file for writing
  File file = SD.open(fullPath.c_str(), FILE_WRITE);
  if (!file) {
    setStatusMessage(StatusMsg::FAILED_TO_SAVE);
    sdCardAvailable = false;
    return false;
  }

  // Write data (291-byte format with version byte)
  file.write(SKETCH_FORMAT_VERSION);  // Version 2
  file.write(activeSketch.gridSize);
  file.write(activeSketch.paletteSize);

  for (int i = 0; i < 16; i++) {
    file.write((activeSketch.paletteColors[i] >> 8) & 0xFF);  // High byte
    file.write(activeSketch.paletteColors[i] & 0xFF);          // Low byte
  }

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      file.write(activeSketch.pixels[y][x]);
    }
  }

  file.close();
  activeSketch.isEmpty = false;

  setStatusMessage(StatusMsg::SAVED);
  return true;
}

/**
 * Save active sketch as NEW copy (creates new timestamped file)
 * Used for Fn+S to duplicate current work
 */
bool saveActiveSketchAsNew() {
  // Force creation of new file regardless of existing filename
  activeSketchFilename = "";
  activeSketchIsNew = true;
  return saveActiveSketchToSD();
}

/**
 * Load a sketch from SD card into activeSketch
 * Filename should be just the filename, not full path
 * Returns true if successful, false if failed
 */
bool loadSketchFromSD(String filename) {
  if (!sdCardAvailable && !initSDCard()) {
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  // Quick card check
  if (SD.cardType() == CARD_NONE) {
    sdCardAvailable = false;
    setStatusMessage(StatusMsg::SD_NOT_READY);
    return false;
  }

  String fullPath = "/bitmap16dx/sketches/" + filename;

  if (!SD.exists(fullPath.c_str())) {
    setStatusMessage(StatusMsg::FILE_NOT_FOUND);
    return false;
  }

  File file = SD.open(fullPath.c_str(), FILE_READ);
  if (!file) {
    setStatusMessage(StatusMsg::FILE_OPEN_FAIL);
    return false;
  }

  // Verify file size and detect format version
  size_t fileSize = file.size();
  uint8_t formatVersion = 1;  // Default to legacy format

  if (fileSize == SKETCH_FILE_SIZE_V2) {
    // New format with version byte
    formatVersion = file.read();
    if (formatVersion != SKETCH_FORMAT_VERSION) {
      // Unknown version, try to read anyway but could fail
      file.close();
      setStatusMessage(StatusMsg::FILE_CORRUPT);
      return false;
    }
  } else if (fileSize == SKETCH_FILE_SIZE_V1) {
    // Legacy format without version byte (version 1)
    formatVersion = 1;
  } else {
    // Invalid file size
    file.close();
    setStatusMessage(StatusMsg::FILE_CORRUPT);
    return false;
  }

  // Read data (same for both versions after version byte)
  activeSketch.gridSize = file.read();
  activeSketch.paletteSize = file.read();

  for (int i = 0; i < 16; i++) {
    uint8_t high = file.read();
    uint8_t low = file.read();
    activeSketch.paletteColors[i] = (high << 8) | low;
  }

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      activeSketch.pixels[y][x] = file.read();
    }
  }

  file.close();

  activeSketch.isEmpty = false;
  activeSketchFilename = filename;
  activeSketchIsNew = false;

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
  int outputSize = scale ? 128 : currentGridSize;
  int pixelScale = scale ? (128 / currentGridSize) : 1;

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
    int canvasY = y / pixelScale;  // Map to canvas coordinate

    for (int x = 0; x < outputSize; x++) {
      int canvasX = x / pixelScale;  // Map to canvas coordinate

      // Get color index from canvas
      uint8_t colorIndex = canvas[canvasY][canvasX];
      uint8_t r, g, b, a;

      if (colorIndex == 0) {
        // Transparent pixel
        r = g = b = 0;
        a = 0;  // Fully transparent
      } else {
        // Get palette color from the active sketch's palette
        uint16_t color565 = activeSketch.paletteColors[colorIndex - 1];

        // Convert RGB565 to RGB888 (using proper conversion with bit expansion)
        r = ((color565 >> 11) & 0x1F);
        r = (r << 3) | (r >> 2);  // Expand 5 bits to 8 bits
        g = ((color565 >> 5) & 0x3F);
        g = (g << 2) | (g >> 4);  // Expand 6 bits to 8 bits
        b = (color565 & 0x1F);
        b = (b << 3) | (b >> 2);  // Expand 5 bits to 8 bits
        a = 255;  // Fully opaque
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

  // Create exports directory if it doesn't exist
  if (!SD.exists("/bitmap16dx/exports")) {
    SD.mkdir("/bitmap16dx/exports");
  }

  // Generate filename with counter
  int exportNum = 0;
  char filename[40];
  do {
    snprintf(filename, sizeof(filename), "/bitmap16dx/exports/dx_%04d.png", exportNum);
    exportNum++;
  } while (SD.exists(filename) && exportNum < 10000);

  if (exportNum >= 10000) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::TOO_MANY_EXPORTS);
    return false;
  }

  // Write to SD card
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::FILE_OPEN_FAIL);
    return false;
  }

  size_t written = file.write(pngBuffer, pngSize);
  file.close();
  free(pngBuffer);

  if (written != (size_t)pngSize) {
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

  // Create screenshots directory if it doesn't exist
  if (!SD.exists("/bitmap16dx/screenshots")) {
    SD.mkdir("/bitmap16dx/screenshots");
  }

  // Generate filename with counter
  int screenshotNum = 0;
  char filename[48];
  do {
    snprintf(filename, sizeof(filename), "/bitmap16dx/screenshots/screenshot_%04d.png", screenshotNum);
    screenshotNum++;
  } while (SD.exists(filename) && screenshotNum < 10000);

  if (screenshotNum >= 10000) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::TOO_MANY_SHOTS);
    return false;
  }

  // Write to SD card
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    free(pngBuffer);
    setStatusMessage(StatusMsg::FILE_OPEN_FAIL);
    return false;
  }

  size_t written = file.write(pngBuffer, pngSize);
  file.close();
  free(pngBuffer);

  if (written != (size_t)pngSize) {
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
  int batteryPercent = M5Cardputer.Power.getBatteryLevel();

  // Redraw if changed or forced
  if (batteryPercent != lastBatteryPercent || forceRedraw) {
    // Clear old icon area (24×24 icon below fill icon)
    M5Cardputer.Display.fillRect(3, 85, 24, 24, currentTheme->background);

    // Select icon based on battery level
    const unsigned char* batteryIcon;
    if (batteryPercent < 10) {
      batteryIcon = ICON_BATTERY_0;  // Low (<10%)
    } else if (batteryPercent < 50) {
      batteryIcon = ICON_BATTERY_10;  // Medium (10-50%)
    } else if (batteryPercent < 90) {
      batteryIcon = ICON_BATTERY_50;  // High (50-90%)
    } else {
      batteryIcon = ICON_BATTERY_90;  // Full (>90%)
    }

    // Draw battery icon at (3, 85) - below fill icon with 4px spacing
    drawIcon(3, 85, batteryIcon, 24, 24, true);

    lastBatteryPercent = batteryPercent;
  }
}

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void drawGrid();
void drawPalette();
void drawCursor();
void drawHelpView();
void drawMemoryView(bool fullRedraw = true);
void drawMemorySketchThumbnail(int slotIndex, int x, int y, int thumbSize);
void drawCreateNewSketchThumbnail(int x, int y, int thumbSize);
void drawSketchThumbnail(int sketchIndex, int x, int y, int thumbSize);
void drawMemoryViewCursor(int itemIndex, int x, int y, int thumbSize);
void updatePaletteFilter();

// ============================================================================
// CANVAS OPERATIONS
// ============================================================================

/**
 * Save current canvas state to undo buffer
 * Note: This is for regular drawing undo, not sketch deletion operations
 */
void saveUndo() {
  for (int y = 0; y < currentGridSize; y++) {
    for (int x = 0; x < currentGridSize; x++) {
      undoCanvas[y][x] = canvas[y][x];
    }
  }
  // Clear palette undo info (this is just a regular drawing undo)
  undoPaletteSize = 0;
  undoGridSize = 0;
  undoAvailable = true;
}

/**
 * Restore canvas from undo buffer
 */
void restoreUndo() {
  if (!undoAvailable) {
    setStatusMessage(StatusMsg::NO_UNDO);
    return;
  }

  // If we have saved grid size info (from sketch deletion), restore it
  if (undoGridSize > 0) {
    currentGridSize = undoGridSize;
    currentCellSize = (currentGridSize == 8) ? 16 : 8;

    // Keep cursor in bounds
    if (cursorX >= currentGridSize) cursorX = currentGridSize - 1;
    if (cursorY >= currentGridSize) cursorY = currentGridSize - 1;
  }

  // Restore all pixels (always restore full 16x16 to handle grid size changes)
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      canvas[y][x] = undoCanvas[y][x];
    }
  }

  // If we have palette info saved (from sketch deletion), restore it to the active sketch
  if (undoPaletteSize > 0) {
    activeSketch.paletteSize = undoPaletteSize;
    activeSketch.gridSize = undoGridSize;
    for (int i = 0; i < 16; i++) {
      activeSketch.paletteColors[i] = undoPaletteColors[i];
    }
  }

  undoAvailable = false;

  // Update LED matrix with restored canvas
  LED_CANVAS_UPDATED();

  setStatusMessage(StatusMsg::UNDO);
}

/**
 * Clear the entire canvas
 * This saves the current state to undo before clearing
 */
void clearCanvas() {
  saveUndo();

  for (int y = 0; y < currentGridSize; y++) {
    for (int x = 0; x < currentGridSize; x++) {
      canvas[y][x] = 0;
    }
  }

  setStatusMessage(StatusMsg::CLEAR);
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
  uint8_t originalColor = canvas[startY][startX];

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
    if (p.x < 0 || p.x >= currentGridSize || p.y < 0 || p.y >= currentGridSize) {
      continue;
    }

    // Skip if this pixel isn't the original color
    if (canvas[p.y][p.x] != originalColor) {
      continue;
    }

    // Fill this pixel
    canvas[p.y][p.x] = fillColor;

    // Add adjacent pixels to stack (4-way connectivity: up, down, left, right)
    // Only add if not visited and within bounds
    // Up
    if (p.y > 0 && !visited[p.y - 1][p.x]) {
      stack[stackSize++] = {p.x, p.y - 1};
      visited[p.y - 1][p.x] = true;
    }
    // Down
    if (p.y < currentGridSize - 1 && !visited[p.y + 1][p.x]) {
      stack[stackSize++] = {p.x, p.y + 1};
      visited[p.y + 1][p.x] = true;
    }
    // Left
    if (p.x > 0 && !visited[p.y][p.x - 1]) {
      stack[stackSize++] = {p.x - 1, p.y};
      visited[p.y][p.x - 1] = true;
    }
    // Right
    if (p.x < currentGridSize - 1 && !visited[p.y][p.x + 1]) {
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
  if (currentGridSize == 8) {
    currentGridSize = 16;
    currentCellSize = 8;  // Smaller cells for more pixels
    setStatusMessage(StatusMsg::GRID_16X16);
  } else {
    currentGridSize = 8;
    currentCellSize = 16;  // Larger cells for fewer pixels
    setStatusMessage(StatusMsg::GRID_8X8);
  }

  // Keep cursor in bounds
  if (cursorX >= currentGridSize) cursorX = currentGridSize - 1;
  if (cursorY >= currentGridSize) cursorY = currentGridSize - 1;

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
  if (activeSketch.paletteSize == 0 || activeSketch.paletteSize > 16) {
    activeSketch.paletteSize = 16;
  }

  // Copy to canvas
  currentGridSize = activeSketch.gridSize;
  currentCellSize = (currentGridSize == 8) ? 16 : 8;

  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      canvas[y][x] = activeSketch.pixels[y][x];
    }
  }

  if (cursorX >= currentGridSize) cursorX = currentGridSize - 1;
  if (cursorY >= currentGridSize) cursorY = currentGridSize - 1;

  selectedColor = 1;

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
      canvas[y][x] = 0;
    }
  }

  currentGridSize = 16;
  currentCellSize = 8;
  cursorX = 0;
  cursorY = 0;
  selectedColor = 1;

  // setStatusMessage("New sketch");  // Removed - no message on boot
}

/**
 * Enter Memory View mode
 */
void enterMemoryView() {
  loadSketchListFromSD();  // Load sketch list (sketch data will be cached on first draw)
  inMemoryView = true;

  // Keep cursor and scroll position persistent between sessions
  // This remembers where you were browsing in the list
  // Just clamp cursor to valid range in case list changed (e.g., deleted items)
  int totalItems = 1 + sketchList.size();
  if (memoryViewCursor >= totalItems) {
    memoryViewCursor = max(0, totalItems - 1);
  }
  if (memoryViewCursor < 0) {
    memoryViewCursor = 0;
  }

  lastMemoryAnimTime = millis();
  memoryCursorAnimPhase = 0.0f;
  M5Cardputer.Display.fillScreen(currentTheme->background);
  drawMemoryView(true);
}

/**
 * Exit Memory View and return to canvas
 */
void exitMemoryView() {
  inMemoryView = false;

  // Redraw the canvas view
  M5Cardputer.Display.fillScreen(currentTheme->background);
  drawGrid();
  drawPalette();
  drawCursor();

  // Redraw icons in upper left corner
  drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
  drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
  drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);
  // drawIcon(3, 84, ICON_INFO, ICON_INFO_WIDTH, ICON_INFO_HEIGHT, ICON_INFO_IS_INDEXED);

  // Redraw battery indicator
  lastBatteryPercent = -1;  // Force redraw
  batteryFirstCheck = true;  // Force immediate check
  drawBatteryIndicator();

}

/**
 * Enter Hint Screen mode
 */
void enterHelpView() {
  // Remember if we're coming from memory view
  helpViewFromMemoryView = inMemoryView;

  inHelpView = true;

  // Draw hint screen
  drawHelpView();
}

/**
 * Exit Hint Screen and return to previous view
 */
void exitHelpView() {
  inHelpView = false;

  // Return to the view we came from
  if (helpViewFromMemoryView) {
    // Return to memory view
    drawMemoryView(true);
    helpViewFromMemoryView = false;
  } else {
    // Return to canvas/drawing view
    M5Cardputer.Display.fillScreen(currentTheme->background);
    drawGrid();
    drawPalette();
    drawCursor();

    // Redraw icons in upper left corner
    drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
    drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
    drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);
    // drawIcon(3, 84, ICON_INFO, ICON_INFO_WIDTH, ICON_INFO_HEIGHT, ICON_INFO_IS_INDEXED);

    // Redraw battery indicator
    lastBatteryPercent = -1;  // Force redraw
    batteryFirstCheck = true;  // Force immediate check
    drawBatteryIndicator();
  }
}

/**
 * Enter View Mode - display canvas at 128×128 with selected background
 */
void enterPreviewView() {
  inPreviewView = true;

  // Get the background color based on current selection
  uint16_t bgColor;
  switch (previewViewBackground) {
    case 0: bgColor = VIEW_BG_BLACK; break;
    case 1: bgColor = VIEW_BG_WHITE; break;
    case 2: bgColor = VIEW_BG_GRAY; break;
    case 3: bgColor = VIEW_BG_DARK; break;
    default: bgColor = VIEW_BG_BLACK; break;
  }

  // Fill screen with selected background color
  M5Cardputer.Display.fillScreen(bgColor);

  // Calculate position to center 128×128 canvas on 240×135 screen
  // X: (240 - 128) / 2 = 56
  // Y: (135 - 128) / 2 = 3.5, use 4 for integer alignment
  const int viewX = 56;
  const int viewY = 4;
  const int viewSize = 128;
  const int viewCellSize = 128 / currentGridSize;  // 16px for 8×8, 8px for 16×16

  // Draw each cell of the canvas
  for (int y = 0; y < currentGridSize; y++) {
    for (int x = 0; x < currentGridSize; x++) {
      int screenX = viewX + (x * viewCellSize);
      int screenY = viewY + (y * viewCellSize);

      // Get color for this cell
      if (canvas[y][x] != 0) {
        // There's a pixel here - use its color from the active sketch's palette
        uint16_t cellColor = activeSketch.paletteColors[canvas[y][x] - 1];
        M5Cardputer.Display.fillRect(screenX, screenY, viewCellSize, viewCellSize, cellColor);
      }
      // If empty (canvas[y][x] == 0), it stays the background color
    }
  }
}

/**
 * Exit View Mode and return to canvas
 */
void exitPreviewView() {
  inPreviewView = false;

  // Redraw the canvas view
  M5Cardputer.Display.fillScreen(currentTheme->background);
  drawGrid();
  drawPalette();
  drawCursor();

  // Redraw icons in upper left corner
  drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
  drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
  drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);
  // drawIcon(3, 84, ICON_INFO, ICON_INFO_WIDTH, ICON_INFO_HEIGHT, ICON_INFO_IS_INDEXED);

  // Redraw battery indicator
  lastBatteryPercent = -1;  // Force redraw
  batteryFirstCheck = true;  // Force immediate check
  drawBatteryIndicator();
}

/**
 * Enter Palette Menu - horizontally scrolling palette selector
 */
void enterPaletteView() {
  inPaletteView = true;

  // Reset filters to "all" when entering palette menu
  paletteFilterSize = 0;
  paletteFilterUser = false;
  updatePaletteFilter();

  // Set cursor to currently active palette (if we can identify it)
  paletteViewCursor = 0;  // Default to first palette
  // Compare active sketch's palette with each catalog palette to find active one
  for (int p = 0; p < totalPaletteCount; p++) {
    bool matches = true;
    for (int c = 0; c < 16; c++) {
      if (activeSketch.paletteColors[c] != pgm_read_word(&allPalettes[p][c])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      // Find this palette in the filtered list
      for (int f = 0; f < filteredPaletteCount; f++) {
        if (filteredPaletteIndices[f] == p) {
          paletteViewCursor = f;
          break;
        }
      }
      break;
    }
  }

  // Initialize scroll position to cursor (no animation on first show)
  paletteViewScrollPos = (float)paletteViewCursor;

  // Clear screen and draw palette menu
  M5Cardputer.Display.fillScreen(currentTheme->background);
}

/**
 * Exit Palette Menu and return to canvas
 */
void exitPaletteView() {
  inPaletteView = false;

  // Redraw the canvas view
  M5Cardputer.Display.fillScreen(currentTheme->background);
  drawGrid();
  drawPalette();
  drawCursor();

  // Redraw icons in upper left corner
  drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
  drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
  drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);
  // drawIcon(3, 84, ICON_INFO, ICON_INFO_WIDTH, ICON_INFO_HEIGHT, ICON_INFO_IS_INDEXED);

  // Redraw battery indicator
  lastBatteryPercent = -1;  // Force redraw
  batteryFirstCheck = true;  // Force immediate check
  drawBatteryIndicator();
}

// ============================================================================
// DRAWING HELPERS
// ============================================================================

// Corner flags for cut corner rectangles
#define CORNER_NONE         0b0000
#define CORNER_TOP_LEFT     0b0001
#define CORNER_TOP_RIGHT    0b0010
#define CORNER_BOTTOM_LEFT  0b0100
#define CORNER_BOTTOM_RIGHT 0b1000
#define CORNER_ALL          0b1111

// Forward declaration
void drawCutCornerRect(int x, int y, int w, int h, int cutSize, uint16_t color, uint8_t corners, M5Canvas* canvas);

/**
 * Helper function to get theme-appropriate cartridge color
 * Maps light theme cartridge colors to dark theme equivalents
 */
inline uint16_t getCartridgeColor(uint16_t originalColor) {
  if (currentTheme == &THEME_DARK) {
    // Map light theme colors to dark theme
    const uint16_t LIGHT_BG = RGB565(0xD3, 0xD3, 0xDD);    // #d3d3dd
    const uint16_t LIGHT_SHADOW = RGB565(0xC1, 0xC4, 0xD6); // #c1c4d6
    const uint16_t DARK_BG = 0x2105;                        // #202226
    const uint16_t DARK_SHADOW = RGB565(0x15, 0x17, 0x1A);  // #15171A

    if (originalColor == LIGHT_BG || originalColor == RGB565(0xD6, 0x9B, 0x00)) { // 0xD69B
      return DARK_BG;
    }
    if (originalColor == LIGHT_SHADOW || originalColor == RGB565(0xC6, 0x3A, 0x00)) { // 0xC63A
      return DARK_SHADOW;
    }
  }
  return originalColor; // Return unchanged in light mode or if no match
}

/**
 * Draw theme-aware cartridge graphic
 * Transforms colors for dark mode on-the-fly
 */
void drawThemedCartridge(int x, int y, M5Canvas* canvas = nullptr) {
  // For light mode, use original graphic directly
  if (currentTheme == &THEME_LIGHT) {
    if (canvas != nullptr) {
      bool oldSwap = canvas->getSwapBytes();
      canvas->setSwapBytes(true);
      canvas->pushImage(x, y, CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT, CARTRIDGE_GRAPHIC);
      canvas->setSwapBytes(oldSwap);
    } else {
      bool oldSwap = M5Cardputer.Display.getSwapBytes();
      M5Cardputer.Display.setSwapBytes(true);
      M5Cardputer.Display.pushImage(x, y, CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT, CARTRIDGE_GRAPHIC);
      M5Cardputer.Display.setSwapBytes(oldSwap);
    }
    return;
  }

  // For dark mode, transform colors
  // Create a temporary buffer for the transformed graphic
  static uint16_t cartridgeBuffer[CARTRIDGE_WIDTH * CARTRIDGE_HEIGHT];

  // Copy and transform colors
  for (int i = 0; i < CARTRIDGE_WIDTH * CARTRIDGE_HEIGHT; i++) {
    cartridgeBuffer[i] = getCartridgeColor(pgm_read_word(&CARTRIDGE_GRAPHIC[i]));
  }

  // Draw the transformed graphic
  if (canvas != nullptr) {
    bool oldSwap = canvas->getSwapBytes();
    canvas->setSwapBytes(true);
    canvas->pushImage(x, y, CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT, cartridgeBuffer);
    canvas->setSwapBytes(oldSwap);
  } else {
    bool oldSwap = M5Cardputer.Display.getSwapBytes();
    M5Cardputer.Display.setSwapBytes(true);
    M5Cardputer.Display.pushImage(x, y, CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT, cartridgeBuffer);
    M5Cardputer.Display.setSwapBytes(oldSwap);
  }
}

/**
 * Draw a single palette preview at given position
 * Shows cartridge graphic with color swatches displayed on it
 * @param canvas Optional canvas to draw to (defaults to main display)
 */
void drawPalettePreview(int x, int y, const uint16_t* palette, bool isCursor, bool isActive, int paletteIndex, M5Canvas* canvas = nullptr) {
  int numColors = allPaletteSizes[paletteIndex];

  // Draw cartridge background graphic (80×92)
  // Center it at the given x, y position
  int cartX = x - (CARTRIDGE_WIDTH / 2);
  int cartY = y - (CARTRIDGE_HEIGHT / 2);

  // Apply insertion animation if active and this is the cursor
  if (paletteInsertionAnimating && isCursor) {
    // Apply quartic ease-in: slow start, accelerates dramatically (gravity/force feel)
    float t = paletteInsertionProgress;
    float eased = t * t * t * t;  // Quartic ease-in

    // Animate cartridge moving DOWN with easing
    cartY = cartY + (int)(PALETTE_INSERT_DISTANCE * eased);
  }

  // Draw theme-aware cartridge graphic
  drawThemedCartridge(cartX, cartY, canvas);

  // Draw color swatches: 64×64 area positioned 8px from left, 6px from top of cartridge
  const int swatchAreaWidth = 64;
  const int swatchAreaHeight = 64;
  int swatchX = cartX + 8;   // 8 pixels from left edge of cartridge
  int swatchY = cartY + 6;   // 6 pixels from top edge of cartridge

  const int cutSize = 2;  // Size of corner cuts

  if (numColors == 4) {
    // 4-color: 1 column × 4 rows, 64×16 each (single wide column)
    const int colorWidth = 64;
    const int colorHeight = 16;

    for (int i = 0; i < 4; i++) {
      int px = swatchX;
      int py = swatchY + (i * colorHeight);

      // Determine which corners to cut based on position
      uint8_t corners = CORNER_NONE;
      if (i == 0) corners = CORNER_TOP_LEFT | CORNER_TOP_RIGHT;        // Top row: cut both top corners
      else if (i == 3) corners = CORNER_BOTTOM_LEFT | CORNER_BOTTOM_RIGHT;  // Bottom row: cut both bottom corners

      drawCutCornerRect(px, py, colorWidth, colorHeight, cutSize, pgm_read_word(&palette[i]), corners, canvas);
    }
  } else if (numColors == 8) {
    // 8-color: 2 colors wide × 4 colors tall = 8 colors, 32×16 each (colors go DOWN)
    const int colorWidth = 32;
    const int colorHeight = 16;

    for (int i = 0; i < 8; i++) {
      int col = i / 4;  // Column changes every 4 colors (0-3 in col 0, 4-7 in col 1)
      int row = i % 4;  // Row cycles 0-3
      int px = swatchX + (col * colorWidth);
      int py = swatchY + (row * colorHeight);

      // Determine which corners to cut based on position
      uint8_t corners = CORNER_NONE;
      if (col == 0 && row == 0) corners = CORNER_TOP_LEFT;           // Top-left swatch
      else if (col == 1 && row == 0) corners = CORNER_TOP_RIGHT;     // Top-right swatch
      else if (col == 0 && row == 3) corners = CORNER_BOTTOM_LEFT;   // Bottom-left swatch
      else if (col == 1 && row == 3) corners = CORNER_BOTTOM_RIGHT;  // Bottom-right swatch

      drawCutCornerRect(px, py, colorWidth, colorHeight, cutSize, pgm_read_word(&palette[i]), corners, canvas);
    }
  } else {
    // 16-color: 4×4 grid, 16×16 each (colors go DOWN columns - VERTICAL LAYOUT)
    const int colorSize = 16;

    for (int i = 0; i < 16; i++) {
      int col = i / 4;  // Column changes every 4 colors
      int row = i % 4;  // Row cycles 0-3
      int px = swatchX + (col * colorSize);
      int py = swatchY + (row * colorSize);

      // Determine which corners to cut based on position in 4×4 grid
      uint8_t corners = CORNER_NONE;
      if (col == 0 && row == 0) corners = CORNER_TOP_LEFT;           // Top-left corner
      else if (col == 3 && row == 0) corners = CORNER_TOP_RIGHT;     // Top-right corner
      else if (col == 0 && row == 3) corners = CORNER_BOTTOM_LEFT;   // Bottom-left corner
      else if (col == 3 && row == 3) corners = CORNER_BOTTOM_RIGHT;  // Bottom-right corner

      drawCutCornerRect(px, py, colorSize, colorSize, cutSize, pgm_read_word(&palette[i]), corners, canvas);
    }
  }

  // No border around cartridge
  // Active palette is indicated by ">" before the name
}

/**
 * Draw Palette Menu - horizontally scrolling carousel of palettes
 * Center palette is selected, left/right arrows navigate
 *
 * @param fullRedraw If true, clears entire screen. If false, only redraws content area for animation
 */
void drawPaletteView(bool fullRedraw = true) {
  // Check if canvas is available
  if (!paletteCanvasAvailable) {
    // Canvas wasn't allocated - show error message
    M5Cardputer.Display.fillScreen(currentTheme->background);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.setCursor(10, 50);
    M5Cardputer.Display.println("WARNING: Low memory!");
    M5Cardputer.Display.setCursor(10, 65);
    M5Cardputer.Display.println("Cannot show palette menu.");
    M5Cardputer.Display.setCursor(10, 85);
    M5Cardputer.Display.setTextColor(currentTheme->text);
    M5Cardputer.Display.println("Press ESC (`) to exit");
    M5Cardputer.Display.setCursor(10, 100);
    M5Cardputer.Display.println("Restart device to recover");
    return;  // Skip drawing, but allow keyboard handling
  }

  // Clear the canvas buffer (full screen)
  paletteCanvas.fillSprite(currentTheme->background);

  // Draw title to canvas
  paletteCanvas.setTextColor(currentTheme->text);
  paletteCanvas.setTextSize(1);
  paletteCanvas.setCursor(4, 4);
  paletteCanvas.print("PALETTES");

  // Draw filter status in top-right corner (only if filters are active)
  if (paletteFilterSize > 0 || paletteFilterUser) {
    String filterText = "";
    if (paletteFilterSize > 0 && paletteFilterUser) {
      filterText = "USER+" + String(paletteFilterSize);
    } else if (paletteFilterSize > 0) {
      filterText = String(paletteFilterSize) + "-COLOR";
    } else if (paletteFilterUser) {
      filterText = "USER";
    }
    // Right-align text: calculate position based on text width
    int16_t textWidth = paletteCanvas.textWidth(filterText);
    paletteCanvas.setCursor(240 - textWidth - 4, 4);  // 4px padding from right edge
    paletteCanvas.print(filterText);
  }

  const int paletteGap = 20;  // Gap between cartridges
  const int centerX = 120;  // Center of 240px screen (horizontal)
  // Cartridge top at Y=20 from screen top, cartridge is 92px tall
  // So center is at Y=20+46=66 (simple screen coordinates now!)
  const int centerY = 66;  // Browsing position: top of cartridge at Y=20 on screen

  // Determine which palette is currently active on the sketch
  int activePaletteIndex = -1;
  // Compare active sketch's palette with each catalog palette
  for (int p = 0; p < totalPaletteCount; p++) {
    bool matches = true;
    for (int c = 0; c < 16; c++) {
      if (activeSketch.paletteColors[c] != pgm_read_word(&allPalettes[p][c])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      activePaletteIndex = p;
      break;
    }
  }

  // Smooth scroll animation - now tear-free thanks to M5Canvas!
  // Skip scroll interpolation during insertion animation (keeps everything frozen)
  if (!paletteInsertionAnimating) {
    // Interpolate scroll position towards cursor position
    float targetPos = (float)paletteViewCursor;
    float diff = targetPos - paletteViewScrollPos;

    if (fabs(diff) > 0.01f) {
      // Smoothly animate towards target
      paletteViewScrollPos += diff * PALETTE_SCROLL_SPEED;
    } else {
      // Close enough - snap to target
      paletteViewScrollPos = targetPos;
    }
  }

  // Draw filtered palettes to canvas - selected one in center, others offset to sides
  for (int i = 0; i < filteredPaletteCount; i++) {
    // Get actual palette index from filtered list
    uint8_t paletteIdx = filteredPaletteIndices[i];

    // During insertion animation, freeze ALL cartridges horizontally using frozen scroll position
    // This keeps everything stable - only vertical (Y) movement happens during insertion
    float scrollPosToUse = paletteInsertionAnimating ? paletteInsertionFrozenScrollPos : paletteViewScrollPos;

    float offset = (i - scrollPosToUse) * (CARTRIDGE_WIDTH + paletteGap);
    int paletteX = centerX + (int)offset;  // X is center point for cartridge
    int paletteY = centerY;

    // Only draw if on screen (with some margin)
    if (paletteX > -(CARTRIDGE_WIDTH / 2) && paletteX < 240 + (CARTRIDGE_WIDTH / 2)) {
      bool isCursor = (i == paletteViewCursor);
      bool isActive = (paletteIdx == activePaletteIndex);

      // Draw palette name FIRST (but not during insertion animation to prevent text peeking out)
      if (isCursor && !paletteInsertionAnimating) {
        paletteCanvas.setTextSize(1);
        paletteCanvas.setTextColor(currentTheme->text);

        // Add checkmark if this is the active palette, star if user-loaded
        String displayText = String(allPaletteNames[paletteIdx]);
        if (paletteIsUserLoaded[paletteIdx]) {
          displayText = "* " + displayText;  // Star for user palettes
        }
        if (isActive) {
          displayText = "> " + displayText;  // Use ">" as checkmark/indicator
        }

        int textWidth = displayText.length() * 6;  // Approximate width
        paletteCanvas.setCursor(centerX - (textWidth / 2), centerY + (CARTRIDGE_HEIGHT / 2) + 6);
        paletteCanvas.print(displayText);
      }

      // Draw cartridge AFTER label (so it covers the label during animation)
      drawPalettePreview(paletteX, paletteY, allPalettes[paletteIdx], isCursor, isActive, paletteIdx, &paletteCanvas);
    }
  }

  // Draw status message at bottom if one exists
  if (statusMessage[0] != '\0' && (millis() - statusMessageTime < STATUS_DISPLAY_DURATION)) {
    paletteCanvas.setTextColor(currentTheme->text);
    paletteCanvas.setTextSize(1);
    paletteCanvas.setCursor(3, 124);
    paletteCanvas.print(statusMessage);
  }

  // NOW push the entire frame to display at once - NO TEARING!
  paletteCanvas.pushSprite(0, 0);  // Push full-screen canvas

  // No instructions at bottom - keeping the interface minimal
}

// ============================================================================
// DRAWING FUNCTIONS
// ============================================================================

/**
 * Draw a 2px offset shadow behind any rectangle with cut corners
 *
 * Shadow is drawn 2px down and 2px right from the specified position.
 * Draw this BEFORE drawing your main element so the element appears on top.
 *
 * For cut corner elements:
 * - Shadow fills behind element's top-left, top-right, and bottom-left cut corners
 * - Shadow's own top-right corner is cut (2px)
 * - Shadow's own bottom-left corner is cut (2px)
 * - Shadow's bottom-right corner remains filled (visible through element's BR cut)
 *
 * @param x X position of the element (shadow will be at x+2)
 * @param y Y position of the element (shadow will be at y+2)
 * @param w Width of the shadow
 * @param h Height of the shadow
 * @param cutCorners Whether to handle cut corners (default: false)
 */
void drawShadow(int x, int y, int w, int h, bool cutCorners = false) {
  // Draw main shadow rectangle (offset 2px down and right)
  // This extends under the element and will show through any cut corners
  M5Cardputer.Display.fillRect(x + 2, y + 2, w, h, currentTheme->shadow);

  if (cutCorners) {
    // Cut the shadow's own visible corners (the parts that stick out beyond the element)
    M5Cardputer.Display.fillRect(x + w, y + 2, 2, 2, currentTheme->background);  // Cut shadow's TR
    M5Cardputer.Display.fillRect(x + 2, y + h, 2, 2, currentTheme->background);  // Cut shadow's BL
    M5Cardputer.Display.fillRect(x + w, y + h, 2, 2, currentTheme->background);  // Cut shadow's BR
  }
}

/**
 * Draw a filled rectangle with optional cut corners
 *
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param cutSize Size of corner cuts in pixels
 * @param color Fill color
 * @param corners Bitfield of which corners to cut (use CORNER_* flags)
 * @param canvas Optional canvas to draw to (defaults to main display)
 */
void drawCutCornerRect(int x, int y, int w, int h, int cutSize, uint16_t color, uint8_t corners = CORNER_ALL, M5Canvas* canvas = nullptr) {
  // Draw main middle section (always full width)
  if (canvas != nullptr) {
    canvas->fillRect(x, y + cutSize, w, h - (cutSize * 2), color);
  } else {
    M5Cardputer.Display.fillRect(x, y + cutSize, w, h - (cutSize * 2), color);
  }

  // Top edge
  int topStart = (corners & CORNER_TOP_LEFT) ? cutSize : 0;
  int topEnd = (corners & CORNER_TOP_RIGHT) ? w - cutSize : w;
  if (canvas != nullptr) {
    canvas->fillRect(x + topStart, y, topEnd - topStart, cutSize, color);
  } else {
    M5Cardputer.Display.fillRect(x + topStart, y, topEnd - topStart, cutSize, color);
  }

  // Bottom edge
  int bottomStart = (corners & CORNER_BOTTOM_LEFT) ? cutSize : 0;
  int bottomEnd = (corners & CORNER_BOTTOM_RIGHT) ? w - cutSize : w;
  if (canvas != nullptr) {
    canvas->fillRect(x + bottomStart, y + h - cutSize, bottomEnd - bottomStart, cutSize, color);
  } else {
    M5Cardputer.Display.fillRect(x + bottomStart, y + h - cutSize, bottomEnd - bottomStart, cutSize, color);
  }
}

/**
 * Draw an outline rectangle with optional cut corners
 *
 * @param x X position
 * @param y Y position
 * @param w Width
 * @param h Height
 * @param cutSize Size of corner cuts in pixels
 * @param color Line color
 * @param corners Bitfield of which corners to cut (use CORNER_* flags)
 */
void drawCutCornerOutline(int x, int y, int w, int h, int cutSize, uint16_t color, uint8_t corners = CORNER_ALL, M5Canvas* canvas = nullptr) {
  // Draw the four edges with proper gaps at corners to create clean cutouts

  // Top edge - skip cutSize pixels on each end where corners are cut
  int topStart = (corners & CORNER_TOP_LEFT) ? cutSize : 0;
  int topEnd = (corners & CORNER_TOP_RIGHT) ? w - cutSize : w;
  if (topEnd > topStart) {
    if (canvas != nullptr) {
      canvas->fillRect(x + topStart, y, topEnd - topStart, 1, color);
    } else {
      M5Cardputer.Display.fillRect(x + topStart, y, topEnd - topStart, 1, color);
    }
  }

  // Bottom edge
  int bottomStart = (corners & CORNER_BOTTOM_LEFT) ? cutSize : 0;
  int bottomEnd = (corners & CORNER_BOTTOM_RIGHT) ? w - cutSize : w;
  if (bottomEnd > bottomStart) {
    if (canvas != nullptr) {
      canvas->fillRect(x + bottomStart, y + h - 1, bottomEnd - bottomStart, 1, color);
    } else {
      M5Cardputer.Display.fillRect(x + bottomStart, y + h - 1, bottomEnd - bottomStart, 1, color);
    }
  }

  // Left edge - skip cutSize pixels on each end where corners are cut
  int leftStart = (corners & CORNER_TOP_LEFT) ? cutSize : 0;
  int leftEnd = (corners & CORNER_BOTTOM_LEFT) ? h - cutSize : h;
  if (leftEnd > leftStart) {
    if (canvas != nullptr) {
      canvas->fillRect(x, y + leftStart, 1, leftEnd - leftStart, color);
    } else {
      M5Cardputer.Display.fillRect(x, y + leftStart, 1, leftEnd - leftStart, color);
    }
  }

  // Right edge
  int rightStart = (corners & CORNER_TOP_RIGHT) ? cutSize : 0;
  int rightEnd = (corners & CORNER_BOTTOM_RIGHT) ? h - cutSize : h;
  if (rightEnd > rightStart) {
    if (canvas != nullptr) {
      canvas->fillRect(x + w - 1, y + rightStart, 1, rightEnd - rightStart, color);
    } else {
      M5Cardputer.Display.fillRect(x + w - 1, y + rightStart, 1, rightEnd - rightStart, color);
    }
  }

  // Corners are left as empty cutouts (cutSize × cutSize squares removed from each corner)
}

/**
 * Blend two RGB565 colors with alpha (0.0 = bg only, 1.0 = fg only)
 */
uint16_t blendRGB565(uint16_t bg, uint16_t fg, float alpha) {
  // Extract RGB components from RGB565
  uint8_t bgR = (bg >> 11) & 0x1F;
  uint8_t bgG = (bg >> 5) & 0x3F;
  uint8_t bgB = bg & 0x1F;

  uint8_t fgR = (fg >> 11) & 0x1F;
  uint8_t fgG = (fg >> 5) & 0x3F;
  uint8_t fgB = fg & 0x1F;

  // Blend
  uint8_t outR = bgR + (fgR - bgR) * alpha;
  uint8_t outG = bgG + (fgG - bgG) * alpha;
  uint8_t outB = bgB + (fgB - bgB) * alpha;

  // Pack back to RGB565
  return (outR << 11) | (outG << 5) | outB;
}

/**
 * Draw a line with alpha transparency by blending a color with existing pixels
 */
void drawLineWithAlpha(int x, int y, int w, int h, uint16_t color, float alpha) {
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      uint16_t bgColor = M5Cardputer.Display.readPixel(x + px, y + py);
      uint16_t blended = blendRGB565(bgColor, color, alpha);
      M5Cardputer.Display.drawPixel(x + px, y + py, blended);
    }
  }
}

/**
 * Draw a single cell at the given grid coordinates
 *
 * This redraws one cell with either:
 * - The pixel color if a pixel is placed there
 * - The checkerboard background if empty (pattern based on screen pixels)
 */
void drawCell(int x, int y, bool isSelected = false) {
  int screenX = GRID_X + (x * currentCellSize);
  int screenY = GRID_Y + (y * currentCellSize);

  // Check if this cell has a color
  if (canvas[y][x] != 0) {
    // FILLED CELL: Draw solid color directly (center lines will be underneath/hidden)
    uint16_t cellColor = activeSketch.paletteColors[canvas[y][x] - 1];

    // Apply tint if this is the selected cell
    if (isSelected) {
      if (currentTheme == &THEME_DARK) {
        // In dark mode, darken the cell (but less than light mode)
        uint8_t r = ((cellColor >> 11) & 0x1F) * 0.7;
        uint8_t g = ((cellColor >> 5) & 0x3F) * 0.7;
        uint8_t b = (cellColor & 0x1F) * 0.7;
        cellColor = (r << 11) | (g << 5) | b;
      } else {
        // In light mode, darken the cell more
        uint8_t r = ((cellColor >> 11) & 0x1F) * 0.8;
        uint8_t g = ((cellColor >> 5) & 0x3F) * 0.8;
        uint8_t b = (cellColor & 0x1F) * 0.8;
        cellColor = (r << 11) | (g << 5) | b;
      }
    }

    M5Cardputer.Display.fillRect(screenX, screenY, currentCellSize, currentCellSize, cellColor);
  } else {
    // EMPTY CELL: Draw checkerboard pattern, then center lines on top
    int checkSize = currentCellSize / 2;

    // Draw checkerboard pattern
    for (int py = 0; py < currentCellSize; py += checkSize) {
      for (int px = 0; px < currentCellSize; px += checkSize) {
        int absX = screenX + px;
        int absY = screenY + py;
        bool isDark = ((absX / checkSize) + (absY / checkSize)) % 2 == 0;
        uint16_t color = isDark ? currentTheme->cellDark : currentTheme->cellLight;

        // Apply tint if this is the selected cell
        if (isSelected) {
          if (currentTheme == &THEME_DARK) {
            // In dark mode, darken the cell (but less than light mode)
            uint8_t r = ((color >> 11) & 0x1F) * 0.7;
            uint8_t g = ((color >> 5) & 0x3F) * 0.7;
            uint8_t b = (color & 0x1F) * 0.7;
            color = (r << 11) | (g << 5) | b;
          } else {
            // In light mode, darken the cell more
            uint8_t r = ((color >> 11) & 0x1F) * 0.8;
            uint8_t g = ((color >> 5) & 0x3F) * 0.8;
            uint8_t b = (color & 0x1F) * 0.8;
            color = (r << 11) | (g << 5) | b;
          }
        }

        int drawWidth = min(checkSize, currentCellSize - px);
        int drawHeight = min(checkSize, currentCellSize - py);
        M5Cardputer.Display.fillRect(absX, absY, drawWidth, drawHeight, color);
      }
    }

    // Draw center lines (if rulers are visible and they pass through this cell)
    if (rulersVisible) {
      int centerX = GRID_X + 64;  // Vertical line at x=120
      int centerY = GRID_Y + 64;  // Horizontal line at y=68

      if (centerX >= screenX && centerX < screenX + currentCellSize) {
        // Redraw the vertical line segment for this cell
        M5Cardputer.Display.fillRect(centerX, screenY, 1, currentCellSize, currentTheme->centerLine);
      }

      if (centerY >= screenY && centerY < screenY + currentCellSize) {
        // Redraw the horizontal line segment for this cell
        M5Cardputer.Display.fillRect(screenX, centerY, currentCellSize, 1, currentTheme->centerLine);
      }
    }
  }

  // LAYER 4: Apply corner masking for corner cells
  if (x == 0 && y == 0) {
    M5Cardputer.Display.fillRect(screenX, screenY, 2, 2, currentTheme->background);
  }
  else if (x == currentGridSize - 1 && y == 0) {
    M5Cardputer.Display.fillRect(screenX + currentCellSize - 2, screenY, 2, 2, currentTheme->background);
  }
  else if (x == 0 && y == currentGridSize - 1) {
    M5Cardputer.Display.fillRect(screenX, screenY + currentCellSize - 2, 2, 2, currentTheme->background);
  }
  else if (x == currentGridSize - 1 && y == currentGridSize - 1) {
    M5Cardputer.Display.fillRect(screenX + currentCellSize - 2, screenY + currentCellSize - 2, 2, 2, currentTheme->shadow);
  }
}

/**
 * Draw the cursor
 *
 * Draws a simple arrow pointer below and to the right of the selected cell
 */
void drawCursor() {
  // Clear the old cursor icon position if it exists
  if (lastCursorScreenX >= 0 && lastCursorScreenY >= 0) {
    // Calculate which cells might have been covered by the old cursor icon
    int oldCursorEndX = lastCursorScreenX + ICON_CANVAS_CURSOR_WIDTH;
    int oldCursorEndY = lastCursorScreenY + ICON_CANVAS_CURSOR_HEIGHT;

    // Find grid cells that overlap with the old cursor area
    int startCellX = max(0, (lastCursorScreenX - GRID_X) / currentCellSize);
    int startCellY = max(0, (lastCursorScreenY - GRID_Y) / currentCellSize);
    int endCellX = min(currentGridSize - 1, (oldCursorEndX - GRID_X) / currentCellSize);
    int endCellY = min(currentGridSize - 1, (oldCursorEndY - GRID_Y) / currentCellSize);

    // First, fill the old cursor area with background color
    M5Cardputer.Display.fillRect(
      lastCursorScreenX,
      lastCursorScreenY,
      ICON_CANVAS_CURSOR_WIDTH,
      ICON_CANVAS_CURSOR_HEIGHT,
      currentTheme->background
    );

    // Then redraw all cells that were covered by the old cursor
    for (int y = startCellY; y <= endCellY && y < currentGridSize; y++) {
      for (int x = startCellX; x <= endCellX && x < currentGridSize; x++) {
        drawCell(x, y);
      }
    }
  }

  // Draw the selected cell with tint
  drawCell(cursorX, cursorY, true);

  // Convert grid coordinates to screen coordinates (top-left of cell)
  int cellX = GRID_X + (cursorX * currentCellSize);
  int cellY = GRID_Y + (cursorY * currentCellSize);

  // Position cursor below and to the right of the cell, with offset for alignment
  int cursorX_pos = cellX + currentCellSize + CURSOR_OFFSET_X;
  int cursorY_pos = cellY + currentCellSize + CURSOR_OFFSET_Y;

  // Draw the cursor icon
  drawIcon(cursorX_pos, cursorY_pos, ICON_CANVAS_CURSOR, ICON_CANVAS_CURSOR_WIDTH, ICON_CANVAS_CURSOR_HEIGHT, ICON_CANVAS_CURSOR_IS_INDEXED);

  // Save this position for next time
  lastCursorScreenX = cursorX_pos;
  lastCursorScreenY = cursorY_pos;
}

/**
 * Draw the palette column with selection indicator
 *
 * Draws color swatches and the selection indicator.
 * This is a simple, complete redraw - no optimization.
 * Layout:
 * - 4-color: single column, right-aligned
 * - 8-color: single column, right-aligned
 * - 16-color: two columns
 */
void drawPalette() {
  // Clear the entire palette area (with a bit of margin for the selection border)
  M5Cardputer.Display.fillRect(
    PALETTE_X - 4,
    GRID_Y - 4,
    PALETTE_WIDTH + 8,
    (PALETTE_SWATCH_SIZE * 8) + 8,
    currentTheme->background
  );

  // Determine layout based on palette size
  // 4-color and 8-color: one column, right-aligned
  // 16-color: two columns
  int numColors = activeSketch.paletteSize;
  int startX = (numColors <= 8) ? (PALETTE_X + PALETTE_SWATCH_SIZE) : PALETTE_X;  // Right-aligned for 4 and 8-color

  // Draw shadow behind the palette (with cut corners)
  int paletteWidth = (numColors <= 8) ? PALETTE_SWATCH_SIZE : PALETTE_WIDTH;
  int paletteHeight = (numColors <= 8) ? (numColors * PALETTE_SWATCH_SIZE) : (8 * PALETTE_SWATCH_SIZE);
  drawShadow(startX, GRID_Y, paletteWidth, paletteHeight, true);

  // Draw color swatches
  for (int i = 0; i < numColors; i++) {
    int col = (numColors <= 8) ? 0 : (i / 8);  // Always column 0 for 4/8-color
    int row = (numColors <= 8) ? i : (i % 8);   // Row 0-3/0-7 for 4/8-color, 0-7 for 16-color

    int swatchX = startX + (col * PALETTE_SWATCH_SIZE);
    int swatchY = GRID_Y + (row * PALETTE_SWATCH_SIZE);

    // Draw the 16×16 color swatch using active sketch's palette
    M5Cardputer.Display.fillRect(
      swatchX,
      swatchY,
      PALETTE_SWATCH_SIZE,
      PALETTE_SWATCH_SIZE,
      activeSketch.paletteColors[i]
    );

    // Apply corner masking for corner swatches (only if NOT currently selected)
    bool isSelected = (i == selectedColor - 1);
    if (!isSelected) {
      if (numColors == 4) {
        // 4-color: single column, right-aligned
        if (i == 0) {
          // Top: mask both top corners
          M5Cardputer.Display.fillRect(swatchX, swatchY, 2, 2, currentTheme->background);  // Top-left
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY, 2, 2, currentTheme->background);  // Top-right
        }
        else if (i == 3) {
          // Bottom: mask both bottom corners
          M5Cardputer.Display.fillRect(swatchX, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->background);  // Bottom-left
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->shadow);  // Bottom-right (shadow!)
        }
      }
      else if (numColors == 8) {
        // 8-color: single column, right-aligned
        if (i == 0) {
          // Top: mask both top corners
          M5Cardputer.Display.fillRect(swatchX, swatchY, 2, 2, currentTheme->background);  // Top-left
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY, 2, 2, currentTheme->background);  // Top-right
        }
        else if (i == 7) {
          // Bottom: mask both bottom corners
          M5Cardputer.Display.fillRect(swatchX, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->background);  // Bottom-left
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->shadow);  // Bottom-right (shadow!)
        }
      } else {
        // 16-color: two columns
        if (i == 0) {
          // Color 1 (index 0): top-left of left column - mask top-left corner
          M5Cardputer.Display.fillRect(swatchX, swatchY, 2, 2, currentTheme->background);
        }
        else if (i == 7) {
          // Color 8 (index 7): bottom-left of left column - mask bottom-left corner
          M5Cardputer.Display.fillRect(swatchX, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->background);
        }
        else if (i == 8) {
          // Color 9 (index 8): top-right of right column - mask top-right corner
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY, 2, 2, currentTheme->background);
        }
        else if (i == 15) {
          // Color 16 (index 15): bottom-right of right column - mask bottom-right corner
          M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY + PALETTE_SWATCH_SIZE - 2, 2, 2, currentTheme->shadow);  // Bottom-right (shadow!)
        }
      }
    }
  }

  // Draw selection indicator on the selected color
  int selectedIndex = selectedColor - 1;
  int col = (numColors <= 8) ? 0 : (selectedIndex / 8);
  int row = (numColors <= 8) ? selectedIndex : (selectedIndex % 8);

  int swatchX = startX + (col * PALETTE_SWATCH_SIZE);
  int swatchY = GRID_Y + (row * PALETTE_SWATCH_SIZE);

  // Draw 2px black outline OUTSIDE the swatch
  M5Cardputer.Display.fillRect(swatchX - 2, swatchY - 2, PALETTE_SWATCH_SIZE + 4, 2, TFT_BLACK);  // Top
  M5Cardputer.Display.fillRect(swatchX - 2, swatchY + PALETTE_SWATCH_SIZE, PALETTE_SWATCH_SIZE + 4, 2, TFT_BLACK);  // Bottom
  M5Cardputer.Display.fillRect(swatchX - 2, swatchY - 2, 2, PALETTE_SWATCH_SIZE + 4, TFT_BLACK);  // Left
  M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE, swatchY - 2, 2, PALETTE_SWATCH_SIZE + 4, TFT_BLACK);  // Right

  // Draw 2px light inset INSIDE the swatch (right against the edge)
  M5Cardputer.Display.fillRect(swatchX, swatchY, PALETTE_SWATCH_SIZE, 2, currentTheme->iconLight);  // Top
  M5Cardputer.Display.fillRect(swatchX, swatchY + PALETTE_SWATCH_SIZE - 2, PALETTE_SWATCH_SIZE, 2, currentTheme->iconLight);  // Bottom
  M5Cardputer.Display.fillRect(swatchX, swatchY, 2, PALETTE_SWATCH_SIZE, currentTheme->iconLight);  // Left
  M5Cardputer.Display.fillRect(swatchX + PALETTE_SWATCH_SIZE - 2, swatchY, 2, PALETTE_SWATCH_SIZE, currentTheme->iconLight);  // Right
}

// ============================================================================
// DRAW MEMORY VIEW GRID - Vertical scrolling (NEW VERSION)
// 4 columns, vertical scrolling through rows
// ============================================================================
void drawMemoryViewGrid(bool fullRedraw = true) {
  const int COLS = 4;         // 4 columns across
  const int thumbSize = 48;   // Each thumbnail is 48×48 pixels
  const int thumbGap = 8;     // Gap between thumbnails horizontally
  const int rowGap = 8;       // Gap between rows vertically
  const int titleHeight = 14; // Height of title area
  const int titleGap = 5;     // Gap between title and first row

  // Total items = 1 (create new button) + number of sketches
  int totalItems = 1 + sketchList.size();

  // Calculate which column and row the cursor is in (4 columns)
  int cursorCol = memoryViewCursor % COLS;
  int cursorRow = memoryViewCursor / COLS;

  // Vertical scrolling logic: scroll to keep cursor row visible
  const int topMargin = 5;     // Small margin at screen top
  const int bottomMargin = 5;  // Space at bottom of screen
  const int itemHeight = thumbSize + rowGap;

  // Calculate cursor position with current TARGET scroll offset
  // Grid starts at: titleHeight + topMargin (title at 0, then topMargin gap)
  int gridStartY = titleHeight + topMargin;
  int cursorScreenY = gridStartY + (cursorRow * itemHeight) - memoryViewScrollOffset;

  // Define scrolling bounds (area where cursor should stay visible)
  // When on first row, we want to scroll back to show the title (scroll offset = 0)
  // So top bound should be where the first row naturally sits when title is visible
  int topBound = titleHeight + topMargin;  // Allow room for title above
  int bottomBound = 135 - bottomMargin - thumbSize;

  // Adjust TARGET scroll offset to keep cursor in bounds
  if (cursorScreenY > bottomBound) {
    // Cursor is past bottom edge - scroll up (increase offset)
    memoryViewScrollOffset += (cursorScreenY - bottomBound);
  } else if (cursorScreenY < topBound) {
    // Cursor is past top edge - scroll down (decrease offset)
    memoryViewScrollOffset -= (topBound - cursorScreenY);
  }

  // Clamp scroll offset to reasonable bounds
  int totalRows = (totalItems + COLS - 1) / COLS;  // Ceiling division
  int totalContentHeight = titleHeight + topMargin + (totalRows * thumbSize) + ((totalRows - 1) * rowGap);
  int visibleHeight = 135 - bottomMargin;  // Can scroll from 0 to bottom
  int maxScroll = max(0, totalContentHeight - visibleHeight);

  if (memoryViewScrollOffset < 0) memoryViewScrollOffset = 0;
  if (memoryViewScrollOffset > maxScroll) memoryViewScrollOffset = maxScroll;

  // Smooth animation: interpolate actual scroll position towards target
  float targetPos = (float)memoryViewScrollOffset;
  float diff = targetPos - memoryViewScrollPos;

  if (fabs(diff) > 0.5f) {
    // Smoothly animate towards target
    memoryViewScrollPos += diff * MEMORY_SCROLL_SPEED;
  } else {
    // Close enough - snap to target
    memoryViewScrollPos = targetPos;
  }

  // Calculate title Y position (scrolls with content)
  // Title starts at 0 (to match PALETTES position), not topMargin
  int titleY = 0 - (int)memoryViewScrollPos;

  // Calculate base Y (where row 0 starts) using animated scroll position
  // Grid starts after title, with topMargin acting as the gap
  int baseY = titleHeight + topMargin - (int)memoryViewScrollPos;

  // Calculate column X positions (horizontally centered)
  // Total width: 4 columns × 48px + 3 gaps × 7px = 213px
  int totalWidth = (COLS * thumbSize) + ((COLS - 1) * thumbGap);
  int startX = (240 - totalWidth) / 2;

  // Create canvas for entire screen to eliminate tearing
  // Memory required: 240×135×2 = 64,800 bytes (~64KB)
  if (!memoryCanvas.createSprite(240, 135)) {
    // Memory allocation failed - show warning once to prevent flashing
    static bool memoryErrorShown = false;
    if (!memoryErrorShown) {
      M5Cardputer.Display.fillScreen(currentTheme->background);
      M5Cardputer.Display.setTextColor(TFT_RED);
      M5Cardputer.Display.setCursor(10, 50);
      M5Cardputer.Display.println("WARNING: Low memory!");
      M5Cardputer.Display.setCursor(10, 65);
      M5Cardputer.Display.println("Cannot display sketches.");
      M5Cardputer.Display.setCursor(10, 85);
      M5Cardputer.Display.setTextColor(currentTheme->text);
      M5Cardputer.Display.println("Press ESC (`) to exit");
      M5Cardputer.Display.setCursor(10, 100);
      M5Cardputer.Display.println("Restart device to recover");
      memoryErrorShown = true;
    }
    return;  // Skip drawing this frame, but allow keyboard handling
  }
  memoryCanvas.fillSprite(currentTheme->background);

  // Draw title to canvas (scrollable position)
  if (titleY > -titleHeight && titleY < 135) {  // Only draw if at least partially visible
    memoryCanvas.setTextColor(currentTheme->text);
    memoryCanvas.setTextSize(1);
    memoryCanvas.setCursor(4, titleY + 4);
    memoryCanvas.print("SKETCHES");
  }

  // Draw all items in grid layout
  for (int itemIndex = 0; itemIndex < totalItems; itemIndex++) {
    int col = itemIndex % COLS;
    int row = itemIndex / COLS;

    int screenX = startX + (col * (thumbSize + thumbGap));
    int screenY = baseY + (row * (thumbSize + rowGap));

    // Only draw if thumbnail is at least partially visible on screen
    if (screenY < -thumbSize - 10 || screenY > 135 + 10) {
      continue;  // Skip off-screen thumbnails
    }

    if (itemIndex == 0) {
      // First item is the "create new" button
      drawCreateNewSketchThumbnail(screenX, screenY, thumbSize);
    } else {
      // Subsequent items are sketches (using cached data!)
      drawSketchThumbnail(itemIndex - 1, screenX, screenY, thumbSize);
    }
  }

  // Draw breathing cursor AFTER all thumbnails (ensures cursor is on top)
  int cursorX = startX + (cursorCol * (thumbSize + thumbGap));
  int cursorY = baseY + (cursorRow * (thumbSize + rowGap));

  // Only draw cursor if it's visible on screen
  if (cursorY >= -thumbSize - 10 && cursorY <= 135 + 10) {
    drawMemoryViewCursor(memoryViewCursor, cursorX, cursorY, thumbSize);
  }

  // Draw status message at bottom if one exists
  if (statusMessage[0] != '\0' && (millis() - statusMessageTime < STATUS_DISPLAY_DURATION)) {
    memoryCanvas.setTextColor(currentTheme->text);
    memoryCanvas.setTextSize(1);
    memoryCanvas.setCursor(3, 124);
    memoryCanvas.print(statusMessage);
  }

  // Push entire canvas to display at (0, 0) to eliminate tearing
  memoryCanvas.pushSprite(0, 0);
  memoryCanvas.deleteSprite();
}

// Helper function to draw a single memory sketch thumbnail (OBSOLETE - kept for compatibility)
// Now replaced by drawCreateNewSketchThumbnail() and drawSketchThumbnail()
void drawMemorySketchThumbnail(int sketchIndex, int x, int y, int thumbSize) {
  // OBSOLETE: This function is no longer used in the unlimited sketches system
  // It's kept as a stub to avoid breaking any remaining references
  // The memory view now uses drawCreateNewSketchThumbnail() and drawSketchThumbnail()
}

// Helper function to draw "+" create new sketch button
void drawCreateNewSketchThumbnail(int x, int y, int thumbSize) {
  // Draw dashed outline
  uint16_t outlineColor = currentTheme->shadow;
  const int cutSize = 2;
  const int dashLength = 4;
  const int gapLength = 4;

  // Draw dotted outline - 2px thick
  // Top edge (skip corners)
  for (int i = cutSize; i < thumbSize - cutSize; i += dashLength + gapLength) {
    int len = min(dashLength, thumbSize - cutSize - i);
    memoryCanvas.fillRect(x + i, y, len, 2, outlineColor);
  }
  // Bottom edge (skip corners)
  for (int i = cutSize; i < thumbSize - cutSize; i += dashLength + gapLength) {
    int len = min(dashLength, thumbSize - cutSize - i);
    memoryCanvas.fillRect(x + i, y + thumbSize - 2, len, 2, outlineColor);
  }
  // Left edge (skip corners)
  for (int i = cutSize; i < thumbSize - cutSize; i += dashLength + gapLength) {
    int len = min(dashLength, thumbSize - cutSize - i);
    memoryCanvas.fillRect(x, y + i, 2, len, outlineColor);
  }
  // Right edge (skip corners)
  for (int i = cutSize; i < thumbSize - cutSize; i += dashLength + gapLength) {
    int len = min(dashLength, thumbSize - cutSize - i);
    memoryCanvas.fillRect(x + thumbSize - 2, y + i, 2, len, outlineColor);
  }

  // Draw "+" symbol in center
  int centerX = x + thumbSize / 2;
  int centerY = y + thumbSize / 2;
  int plusSize = 15;
  int plusThickness = 3;

  memoryCanvas.fillRect(centerX - plusThickness/2, centerY - plusSize/2,
                        plusThickness, plusSize, currentTheme->text);
  memoryCanvas.fillRect(centerX - plusSize/2, centerY - plusThickness/2,
                        plusSize, plusThickness, currentTheme->text);
}

// Helper function to draw sketch thumbnail using cached data
void drawSketchThumbnail(int sketchIndex, int x, int y, int thumbSize) {
  if (sketchIndex < 0 || sketchIndex >= sketchList.size()) {
    return;
  }

  SketchInfo& info = sketchList[sketchIndex];

  // Load data from SD if not already cached
  if (!info.dataLoaded) {
    String fullPath = "/bitmap16dx/sketches/" + info.filename;
    File file = SD.open(fullPath.c_str(), FILE_READ);
    if (!file) {
      setStatusMessage(StatusMsg::FILE_OPEN_FAIL);
      return;
    }

    // Verify file size and detect format version
    size_t fileSize = file.size();
    uint8_t formatVersion = 1;

    if (fileSize == SKETCH_FILE_SIZE_V2) {
      // New format with version byte
      formatVersion = file.read();
      if (formatVersion != SKETCH_FORMAT_VERSION) {
        file.close();
        return;
      }
    } else if (fileSize == SKETCH_FILE_SIZE_V1) {
      // Legacy format without version byte
      formatVersion = 1;
    } else {
      // Invalid file size
      file.close();
      return;
    }

    // Read sketch data into cache
    info.sketchData.gridSize = file.read();
    info.sketchData.paletteSize = file.read();

    for (int i = 0; i < 16; i++) {
      uint8_t high = file.read();
      uint8_t low = file.read();
      info.sketchData.paletteColors[i] = (high << 8) | low;
    }

    for (int py = 0; py < 16; py++) {
      for (int px = 0; px < 16; px++) {
        info.sketchData.pixels[py][px] = file.read();
      }
    }

    file.close();
    info.dataLoaded = true;  // Mark as cached
  }

  // Use cached data to render thumbnail
  Sketch& tempSketch = info.sketchData;

  int cellSize = (tempSketch.gridSize == 8) ? 6 : 3;
  int gridPixelSize = tempSketch.gridSize * cellSize;
  int offsetX = (thumbSize - gridPixelSize) / 2;
  int offsetY = (thumbSize - gridPixelSize) / 2;

  for (int py = 0; py < tempSketch.gridSize; py++) {
    for (int px = 0; px < tempSketch.gridSize; px++) {
      uint8_t pixelIndex = tempSketch.pixels[py][px];
      if (pixelIndex == 0) continue;

      uint16_t color = tempSketch.paletteColors[pixelIndex - 1];
      memoryCanvas.fillRect(x + offsetX + (px * cellSize),
                           y + offsetY + (py * cellSize),
                           cellSize, cellSize, color);
    }
  }

  // Cut corners
  const int cutSize = 2;
  uint16_t bgColor = currentTheme->background;
  memoryCanvas.fillRect(x, y, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x + thumbSize - cutSize, y, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x, y + thumbSize - cutSize, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x + thumbSize - cutSize, y + thumbSize - cutSize, cutSize, cutSize, bgColor);

  // Draw yellow border if this is the currently active sketch
  if (info.filename == activeSketchFilename && !activeSketchIsNew) {
    memoryCanvas.drawRect(x - 1, y - 1, thumbSize + 2, thumbSize + 2, TFT_YELLOW);
  }
}

// Helper function to draw breathing cursor animation on selected item
void drawMemoryViewCursor(int itemIndex, int x, int y, int thumbSize) {
  if (memoryViewCursor != itemIndex) {
    return;  // Not the selected item
  }

  // Breathing cursor animation using corner icons
  auto drawCorner = [&](int cornerX, int cornerY, bool flipH, bool flipV) {
    for (int row = 0; row < ICON_SELECTOR_CORNER_HEIGHT; row++) {
      for (int col = 0; col < ICON_SELECTOR_CORNER_WIDTH; col++) {
        int pixelIndex = row * ICON_SELECTOR_CORNER_WIDTH + col;
        int byteIndex = pixelIndex / 4;
        int bitShift = (3 - (pixelIndex % 4)) * 2;

        uint8_t byte = pgm_read_byte(&ICON_SELECTOR_CORNER[byteIndex]);
        uint8_t value = (byte >> bitShift) & 0x03;

        if (value != 0) {
          int drawX = flipH ? (cornerX + ICON_SELECTOR_CORNER_WIDTH - 1 - col) : (cornerX + col);
          int drawY = flipV ? (cornerY + ICON_SELECTOR_CORNER_HEIGHT - 1 - row) : (cornerY + row);

          uint16_t color = (value == 1) ? currentTheme->iconDark : currentTheme->iconLight;
          memoryCanvas.drawPixel(drawX, drawY, color);
        }
      }
    }
  };

  // Animated breathing effect
  float sineWave = sin(memoryCursorAnimPhase * 2.0f * PI);
  float breathCycle = (sineWave + 1.0f) * 0.5f;
  int offsetX = (int)(breathCycle * 4.0f + 0.5f);
  int offsetY = (int)(breathCycle * 4.0f + 0.5f);

  // Clear corners for cursor animation
  const int cutSize = 2;
  uint16_t bgColor = currentTheme->background;
  memoryCanvas.fillRect(x, y, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x + thumbSize - cutSize, y, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x, y + thumbSize - cutSize, cutSize, cutSize, bgColor);
  memoryCanvas.fillRect(x + thumbSize - cutSize, y + thumbSize - cutSize, cutSize, cutSize, bgColor);

  // Draw animated corners (closer to thumbnail - reduced from 14 to 6 pixels away)
  const int cornerOffset = 6;  // Distance from thumbnail edge
  drawCorner(x - cornerOffset + offsetX, y - cornerOffset + offsetY, false, false);
  drawCorner(x + thumbSize + cornerOffset - 16 - offsetX, y - cornerOffset + offsetY, true, false);
  drawCorner(x - cornerOffset + offsetX, y + thumbSize + cornerOffset - 16 - offsetY, false, true);
  drawCorner(x + thumbSize + cornerOffset - 16 - offsetX, y + thumbSize + cornerOffset - 16 - offsetY, true, true);
}



// ============================================================================
// DRAW MEMORY VIEW 
// ============================================================================
void drawMemoryView(bool fullRedraw) {
  drawMemoryViewGrid(fullRedraw);
}

/**
 * Draw Hint Screen - displays all keyboard controls
 */
void drawHelpView() {
  M5Cardputer.Display.fillScreen(currentTheme->background);

  // Title
  M5Cardputer.Display.setTextColor(currentTheme->text);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 4);
  M5Cardputer.Display.print("HINTS");

  // Draw help text in columns
  int leftCol = 4;
  int rightCol = 125;
  int startY = 20;
  int lineHeight = 10;
  int currentLine = 0;

  // Left column - Drawing controls
  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("DRAWING");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Move: Arrows");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Draw: Ok");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Erase: Del");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Fill: F");
  currentLine++;

  // Space between sections
  currentLine++;

  // Colors section
  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("COLORS");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Color 1-8: 1-8");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Color 9-16: Fn+1-8");
  currentLine++;

  M5Cardputer.Display.setCursor(leftCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Swap Palette: P");
  currentLine++;

  // Right column - System controls
  currentLine = 0;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("SYSTEM");
  currentLine++;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Open: O");
  currentLine++;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Undo: Z");
  currentLine++;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Save: S");
  currentLine++;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Export: X");
  currentLine++;

  M5Cardputer.Display.setCursor(rightCol, startY + (currentLine * lineHeight));
  M5Cardputer.Display.print("Clear: G0");
  currentLine++;
}

/**
 * Draw the grid with checkerboard pattern
 *
 * This function draws a 2×2 checkerboard pattern within each cell
 * to create a finer transparency grid, with cut corners.
 */
void drawGrid() {
  // Draw shadow behind the grid (with bottom-right cut corner)
  drawShadow(GRID_X, GRID_Y, 128, 128, true);

  // Draw each cell
  for (int y = 0; y < currentGridSize; y++) {
    for (int x = 0; x < currentGridSize; x++) {
      drawCell(x, y);
    }
  }

  // Cut the corners by drawing background color over them
  // Canvas is 128×128, positioned at GRID_X, GRID_Y
  // BR corner shows shadow color (reveals the shadow underneath)
  M5Cardputer.Display.fillRect(GRID_X, GRID_Y, 2, 2, currentTheme->background);                          // Top-left
  M5Cardputer.Display.fillRect(GRID_X + 128 - 2, GRID_Y, 2, 2, currentTheme->background);                // Top-right
  M5Cardputer.Display.fillRect(GRID_X, GRID_Y + 128 - 2, 2, 2, currentTheme->background);                // Bottom-left
  M5Cardputer.Display.fillRect(GRID_X + 128 - 2, GRID_Y + 128 - 2, 2, 2, currentTheme->shadow);                // Bottom-right (shadow color!)
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

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      // Check for backtick (ESC key)
      for (auto i : M5Cardputer.Keyboard.keysState().word) {
        if (i == '`') {
          waiting = false;
          break;
        }
      }
    }
    delay(10);  // Small delay to prevent busy-waiting
  }
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
  File file = SD.open(filepath);
  if (!file) return false;

  uint8_t colorCount = 0;

  while (file.available() && colorCount < 16) {
    String line = file.readStringUntil('\n');
    line.trim();

    // Skip empty lines and comments
    if (line.length() == 0 || line.startsWith("//")) continue;

    // Remove # prefix if present
    if (line.startsWith("#")) line = line.substring(1);

    // Must be 6-char hex code
    if (line.length() != 6) continue;

    // Parse RGB hex
    long rgb = strtol(line.c_str(), NULL, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    // Convert to RGB565
    colors[colorCount++] = RGB565(r, g, b);
  }

  file.close();

  // Validate size (must be exactly 4, 8, or 16)
  if (colorCount != 4 && colorCount != 8 && colorCount != 16) {
    return false;
  }

  // Wrap remaining slots cyclically (for 4/8 color palettes)
  // For 8-color palette: indices 9-16 map to colors 1-8
  // For 4-color palette: indices 5-16 map to colors 1-4 repeated
  for (uint8_t i = colorCount; i < 16; i++) {
    colors[i] = colors[i % colorCount];
  }

  *size = colorCount;
  return true;
}

// Load user palettes from SD card /bitmap16dx/palettes/ folder
void loadUserPalettes() {
  // Check if /bitmap16dx/palettes folder exists
  File root = SD.open("/bitmap16dx/palettes");
  if (!root || !root.isDirectory()) {
    // Try to create the folder
    if (SD.mkdir("/bitmap16dx/palettes")) {
      // Try opening again
      root = SD.open("/bitmap16dx/palettes");
      if (!root || !root.isDirectory()) {
        return;
      }
    } else {
      return;
    }
  }

  File file = root.openNextFile();
  while (file && totalPaletteCount < 32) {
    String filename = String(file.name());

    // Only process .hex files
    if (!file.isDirectory() && filename.endsWith(".hex")) {
      // Allocate memory for this palette
      uint16_t* colors = (uint16_t*)malloc(16 * sizeof(uint16_t));
      char* name = (char*)malloc(32);
      uint8_t size;

      // Try to load palette
      String filepath = String("/bitmap16dx/palettes/") + filename;
      if (loadPaletteFromHex(filepath.c_str(), colors, &size)) {
        // Extract name from filename (remove .hex, convert to uppercase)
        String paletteName = filename.substring(0, filename.length() - 4);
        paletteName.toUpperCase();
        paletteName.replace("-", " ");
        paletteName.replace("_", " ");
        strncpy(name, paletteName.c_str(), 31);
        name[31] = '\0';

        // Add to catalog
        allPalettes[totalPaletteCount] = colors;
        allPaletteNames[totalPaletteCount] = name;
        allPaletteSizes[totalPaletteCount] = size;
        paletteIsUserLoaded[totalPaletteCount] = true;

        totalPaletteCount++;
      } else {
        // Invalid palette - free memory and skip silently
        free(colors);
        free(name);
      }
    }

    file = root.openNextFile();
  }

  root.close();
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

/**
 * Convert 2D canvas coordinates to linear LED index.
 * Simple linear mapping - no mirroring, no serpentine.
 */
uint8_t getLEDIndex(uint8_t x, uint8_t y) {
    // Straight linear mapping: left to right, top to bottom
    return y * 8 + x;
}

/**
 * Convert RGB565 color to RGB888 for WS2812 LEDs.
 * RGB565: 5 bits red, 6 bits green, 5 bits blue (16-bit)
 * RGB888: 8 bits per channel (24-bit)
 * This expands the color depth from 65K to 16.7M colors.
 */
CRGB rgb565ToRGB888(uint16_t rgb565) {
    uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;  // 5-bit → 8-bit
    uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;   // 6-bit → 8-bit
    uint8_t b = (rgb565 & 0x1F) * 255 / 31;          // 5-bit → 8-bit
    return CRGB(r, g, b);
}

/**
 * Update the LED matrix to mirror the 8×8 canvas.
 * Only updates when:
 *   - LED matrix is enabled
 *   - Grid size is 8×8 (not 16×16)
 *   - Canvas has changed since last update
 */
void updateLEDMatrix() {
    // Only update in 8×8 mode (16×16 mode doesn't fit on 8×8 matrix)
    if (!ledMatrixEnabled || currentGridSize != 8) {
        // Turn off all LEDs if disabled or in 16×16 mode
        FastLED.clear();
        FastLED.show();
        return;
    }

    // Mirror each canvas cell to the corresponding LED
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            uint8_t pixelValue = canvas[y][x];
            uint8_t ledIndex = getLEDIndex(x, y);
            bool isCursor = (x == cursorX && y == cursorY);

            if (pixelValue == 0) {
                // Empty cell: show dim white for cursor, black otherwise
                leds[ledIndex] = isCursor ? CRGB(40, 40, 40) : CRGB::Black;
            } else {
                // Filled cell: use palette color (index 1-16)
                uint16_t rgb565 = activeSketch.paletteColors[pixelValue - 1];
                CRGB color = rgb565ToRGB888(rgb565);

                // Brighten cursor position by adding white
                if (isCursor) {
                    color.r = min(255, color.r + 80);
                    color.g = min(255, color.g + 80);
                    color.b = min(255, color.b + 80);
                }

                leds[ledIndex] = color;
            }
        }
    }

    // Update the physical LEDs
    FastLED.show();
}

/**
 * Toggle LED matrix on/off with visual feedback.
 * Shows brief status message on main display.
 */
void toggleLEDMatrix() {
    ledMatrixEnabled = !ledMatrixEnabled;

    // Save preference
    preferences.begin("bitmap16dx", false);
    preferences.putBool("ledEnabled", ledMatrixEnabled);
    preferences.end();

    // Visual feedback
    if (ledMatrixEnabled) {
        // Show a brief "DX" pattern to confirm hardware is working
        // Pattern displays as a simple checkmark/confirmation graphic
        FastLED.clear();

        // Set brightness to 10% for startup pattern
        FastLED.setBrightness((10 * 255) / 100);

        // Define the "DX" pattern LEDs
        // Pattern:  . X X .   X . X .
        //           . X . X   . X . .
        //           . X X .   X . X .
        const int dxPattern[] = {9, 10, 17, 19, 25, 26, 36, 38, 45, 52, 54};
        const int patternSize = 11;

        // Light up the pattern with white
        for (int i = 0; i < patternSize; i++) {
            leds[dxPattern[i]] = CRGB::White;
        }
        FastLED.show();
        delay(1000);  // Hold pattern for 1 second

        // Restore user's brightness setting
        FastLED.setBrightness((ledBrightness * 255) / 100);

        // Turn on: immediately update LEDs with current canvas
        LED_CANVAS_UPDATED();
        updateLEDMatrix();
    } else {
        // Turn off: clear all LEDs
        FastLED.clear();
        FastLED.show();
    }
}

/**
 * Adjust LED matrix brightness.
 * @param delta: +5 to increase, -5 to decrease
 */
void adjustLEDBrightness(int8_t delta) {
    if (!ledMatrixEnabled) return;  // No-op if disabled

    // Adjust brightness in 5% increments
    int16_t newBrightness = ledBrightness + delta;

    // Clamp to valid range
    if (newBrightness < MIN_LED_BRIGHTNESS) {
        newBrightness = MIN_LED_BRIGHTNESS;
    } else if (newBrightness > MAX_LED_BRIGHTNESS) {
        newBrightness = MAX_LED_BRIGHTNESS;
    }

    ledBrightness = (uint8_t)newBrightness;

    // Apply new brightness (FastLED uses 0-255 scale)
    FastLED.setBrightness((ledBrightness * 255) / 100);
    FastLED.show();  // Refresh LEDs with new brightness

    // Save preference
    preferences.begin("bitmap16dx", false);
    preferences.putUChar("ledBright", ledBrightness);
    preferences.end();
}
#endif // ENABLE_LED_MATRIX

// setup() runs once when the device boots
void setup() {
  // Initialize the M5Cardputer hardware
  // This turns on the screen, keyboard, speaker, etc.
  auto cfg = M5.config();

  // Disable auto-sleep so device stays awake during use
  cfg.internal_rtc = false;  // Disable RTC-based power management
  cfg.external_rtc = false;  // Disable external RTC if present

  M5Cardputer.begin(cfg);

  // Detect which Cardputer model is running
  // This helps with debugging and user support
  auto boardType = M5.getBoard();
  if (boardType == m5::board_t::board_M5Cardputer) {
    detectedBoardName = "M5Cardputer";
  } else if (boardType == m5::board_t::board_M5CardputerADV) {
    detectedBoardName = "M5Cardputer ADV";
  }

  // Load saved brightness setting from preferences
  // Default to 80% if never saved before
  preferences.begin("bitmap16dx", false);
  displayBrightness = preferences.getUChar("brightness", 80);

  // Load theme preference (default to light mode)
  bool darkMode = preferences.getBool("darkMode", false);
  currentTheme = darkMode ? &THEME_DARK : &THEME_LIGHT;

  preferences.end();

  // Set initial display brightness
  // displayBrightness is stored as percentage (10-100%), convert to hardware range (0-255)
  uint8_t hardwareBrightness = (displayBrightness * 255) / 100;
  M5Cardputer.Display.setBrightness(hardwareBrightness);

#if ENABLE_LED_MATRIX
  // Initialize LED matrix (8×8 WS2812E RGB LEDs)
  preferences.begin("bitmap16dx", true);  // Read-only
  ledMatrixEnabled = preferences.getBool("ledEnabled", false);  // Default: OFF
  ledBrightness = preferences.getUChar("ledBright", DEFAULT_LED_BRIGHTNESS);
  preferences.end();

  // Configure FastLED for WS2812 LEDs
  // WS2812E uses GRB color order
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness((ledBrightness * 255) / 100);
  FastLED.clear();
  FastLED.show();
#endif // ENABLE_LED_MATRIX

  // Initialize palette system
  initStockPalettes();

  // Initialize SD card and load user palettes
  if (initSDCard()) {
    loadUserPalettes();
  }

  // Show boot screen with logo
  showBootScreen();


  // Create off-screen buffer for palette menu (eliminates screen tearing)
  // Size: 240×135 pixels (full screen for easy coordinate mapping)
  // Using 16-bit color for accurate colors (frame limiting handles performance)
  // Memory required: 240×135×2 = 64,800 bytes (~64KB)
  if (!paletteCanvas.createSprite(240, 135)) {
    // Memory allocation failed - set flag and continue without canvas
    paletteCanvasAvailable = false;
    // Device will boot normally, palette menu will show error if accessed
  } else {
    paletteCanvasAvailable = true;
  }

  // Initialize active sketch as blank
  initializeActiveSketch();

  // Create new blank sketch (always start with empty canvas)
  createNewSketch();

  // Clear the screen to background color
  M5Cardputer.Display.fillScreen(currentTheme->background);

  // Pre-clear the status message area (left of grid) so future fillRects look consistent
  // Grid starts at x=56, so clear from x=3 to x=55 (width=53)
  M5Cardputer.Display.fillRect(3, 124, 53, 11, currentTheme->background);

  // Boot directly to Draw View (not Memory View)
  // Draw the grid first (fills the area)
  drawGrid();

  // Draw the palette column
  drawPalette();

  // Draw the initial cursor
  drawCursor();

  // Draw icons in upper left corner
  drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
  drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
  drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);
  // drawIcon(3, 84, ICON_INFO, ICON_INFO_WIDTH, ICON_INFO_HEIGHT, ICON_INFO_IS_INDEXED);

  // Draw initial battery indicator
  drawBatteryIndicator();
}

// ============================================================================
// VIEW HANDLER FUNCTIONS
// ============================================================================

/**
 * Handle Help View input and rendering
 */
void handleHelpView(Keyboard_Class::KeysState& status) {
  // Handle help view controls - ESC or I to exit
  if (M5Cardputer.Keyboard.isPressed()) {
    // Check for character keys
    for (auto i : status.word) {
      // ` key (ESC) or I key - exit help view
      if (i == '`' || i == 'i' || i == 'I') {
        exitHelpView();
        delay(200);  // Debounce
        return;
      }
#if ENABLE_SCREENSHOTS
      // Y key - Take Screenshot
      else if (i == 'y' || i == 'Y') {
        takeScreenshot();
        enterHelpView();  // Redraw help view after screenshot status message
      }
#endif
    }
  }

  delay(10);
}

/**
 * Handle Memory View input and rendering
 */
void handleMemoryView(Keyboard_Class::KeysState& status) {
  // Handle memory view controls
  static bool memoryViewNeedsRedraw = true;
  static int lastMemoryViewCursor = -1;

  // Check if scroll animation is in progress
  bool isScrolling = fabs(memoryViewScrollPos - (float)memoryViewScrollOffset) > 0.5f;

  // Check if cursor breathing animation should redraw
  // Always animate breathing for visual feedback
  bool needsCursorAnimationRedraw = true;

  // Redraw if cursor moved, first time, scrolling, or cursor animation active
  if (memoryViewNeedsRedraw || lastMemoryViewCursor != memoryViewCursor || isScrolling || needsCursorAnimationRedraw) {
    // Throttle animation redraws to maintain smooth framerate (always throttle, not just when scrolling)
    unsigned long now = millis();
    if (now - lastMemoryAnimTime < MEMORY_ANIM_FRAME_MS) {
      // Skip this frame, too soon
    } else {
      // Update cursor animation phase using time-based animation (eliminates frame timing issues)
      // Calculate phase directly from time for perfectly smooth animation
      unsigned long deltaTime = now - lastMemoryAnimTime;

      // Clamp deltaTime to prevent huge jumps (max 100ms = ~10 dropped frames)
      if (deltaTime > 100) deltaTime = 100;

      float timeIncrement = (float)deltaTime / 1000.0f * MEMORY_CURSOR_ANIM_SPEED * 60.0f;  // Scale to ~60fps equivalent
      memoryCursorAnimPhase += timeIncrement;
      while (memoryCursorAnimPhase > 1.0f) {
        memoryCursorAnimPhase -= 1.0f;  // Loop back to 0 (use while in case of large increment)
      }

      if (memoryViewNeedsRedraw) {
        // Full redraw (includes title and thumbnails)
        drawMemoryView(true);
        memoryViewNeedsRedraw = false;
      } else {
        // Cursor moved or animating - always redraw during animation
        drawMemoryView(false);
      }
      lastMemoryViewCursor = memoryViewCursor;
      lastMemoryAnimTime = now;
    }
  }

  // Check for G0 button - delete selected sketch (if not on "+")
  if (M5Cardputer.BtnA.wasPressed() && memoryViewCursor > 0) {
    int sketchIndex = memoryViewCursor - 1;
    if (sketchIndex < sketchList.size()) {
      // Save sketch to undo buffer before deleting (so we can restore with Z)
      Sketch& sketchData = sketchList[sketchIndex].sketchData;

      // Copy pixel data to undo buffer
      for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
          undoCanvas[y][x] = sketchData.pixels[y][x];
        }
      }

      // Copy palette and grid info to undo buffer
      for (int i = 0; i < 16; i++) {
        undoPaletteColors[i] = sketchData.paletteColors[i];
      }
      undoPaletteSize = sketchData.paletteSize;
      undoGridSize = sketchData.gridSize;
      undoAvailable = true;

      // Now delete the file
      String filename = "/bitmap16dx/sketches/" + sketchList[sketchIndex].filename;
      SD.remove(filename.c_str());
      loadSketchListFromSD();  // Refresh list (clears cached data)

      // Move cursor if we deleted the last item
      int totalItems = 1 + sketchList.size();
      if (memoryViewCursor >= totalItems) {
        memoryViewCursor = totalItems - 1;
      }
    }
    memoryViewNeedsRedraw = true;
    lastMemoryViewCursor = -1;
    delay(200);  // Debounce
  }

  if (M5Cardputer.Keyboard.isPressed()) {
    // Enter key - create new sketch or open selected sketch
    if (status.enter) {
      if (memoryViewCursor == 0) {
        // Create new blank sketch
        createNewSketch();
      } else {
        // Open selected sketch
        int sketchIndex = memoryViewCursor - 1;
        if (sketchIndex < sketchList.size()) {
          openSketch(sketchList[sketchIndex].filename);
        }
      }
      exitMemoryView();
      memoryViewNeedsRedraw = true;
      lastMemoryViewCursor = -1;
      delay(200);  // Debounce
      return;
    }

    // Check for character keys
    for (auto i : status.word) {
      // Z key - Undo (restore last cleared sketch from memory view)
      if (i == 'z' || i == 'Z') {
        if (undoAvailable) {
          // Restore the undo buffer to active sketch
          // (This restores canvas-level undo, not sketch deletion)

          // If we have saved grid size info, restore it
          if (undoGridSize > 0) {
            currentGridSize = undoGridSize;
            currentCellSize = (currentGridSize == 8) ? 16 : 8;

            // Keep cursor in bounds
            if (cursorX >= currentGridSize) cursorX = currentGridSize - 1;
            if (cursorY >= currentGridSize) cursorY = currentGridSize - 1;
          }

          // Restore pixel data to canvas
          for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
              canvas[y][x] = undoCanvas[y][x];
            }
          }

          // Restore palette information to active sketch
          activeSketch.paletteSize = undoPaletteSize;
          activeSketch.gridSize = undoGridSize;
          for (int i = 0; i < 16; i++) {
            activeSketch.paletteColors[i] = undoPaletteColors[i];
          }

          activeSketch.isEmpty = false;
          undoAvailable = false;

          // Save restored sketch to SD card (now uses active sketch system)
          // Copy canvas to active sketch before saving
          for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
              activeSketch.pixels[y][x] = canvas[y][x];
            }
          }
          activeSketch.gridSize = currentGridSize;
          saveActiveSketchToSD();

          // Reload sketch list to show the restored sketch
          loadSketchListFromSD();

          setStatusMessage(StatusMsg::RESTORED_SKETCH);
          memoryViewNeedsRedraw = true;
          lastMemoryViewCursor = -1;
          delay(200);  // Debounce
        } else {
          setStatusMessage(StatusMsg::NO_UNDO);
          delay(200);  // Debounce
        }
      }
      // ` key (ESC) or O key - exit memory view
      else if (i == '`' || i == 'o' || i == 'O') {
        // Always allow exiting memory view (can go back to current sketch)
        exitMemoryView();
        memoryViewNeedsRedraw = true;
        lastMemoryViewCursor = -1;
        delay(200);  // Debounce
        return;
      }
      // I key - Open help view
      else if (i == 'i' || i == 'I') {
        enterHelpView();
        delay(200);  // Debounce
        return;  // Exit memory view loop to enter help view mode
      }
#if ENABLE_SCREENSHOTS
      // Y key - Take Screenshot
      else if (i == 'y' || i == 'Y') {
        takeScreenshot();
        memoryViewNeedsRedraw = true;  // Redraw after screenshot status message
      }
#endif
      // Arrow keys - navigate grid (2D navigation with 4 columns)
      const int COLS = 4;  // Match the column count in drawMemoryViewGrid
      int totalItems = 1 + sketchList.size();  // 1 for "+", rest are sketches

      if (i == ';' && memoryViewCursor >= COLS) {  // Up
        memoryViewCursor -= COLS;
        delay(150);
      }
      else if (i == '.') {  // Down
        int currentCol = memoryViewCursor % COLS;  // Which column are we in?
        int nextRow = memoryViewCursor + COLS;

        // If moving down would go past the last item
        if (nextRow >= totalItems) {
          // Calculate the first item in the last row
          int lastRowStart = ((totalItems - 1) / COLS) * COLS;
          // Try to maintain the same column, or clamp to the last item
          int targetPos = lastRowStart + currentCol;
          if (targetPos >= totalItems) {
            targetPos = totalItems - 1;  // Clamp to last item
          }
          memoryViewCursor = targetPos;
        } else {
          // Normal move down
          memoryViewCursor = nextRow;
        }
        delay(150);
      }
      else if (i == ',' && memoryViewCursor % COLS != 0) {  // Left
        memoryViewCursor--;
        delay(150);
      }
      else if (i == '/' && memoryViewCursor % COLS != (COLS - 1) && memoryViewCursor < totalItems - 1) {  // Right
        memoryViewCursor++;
        delay(150);
      }
    }
  }

  delay(10);
}

/**
 * Handle Preview View input and rendering
 */
void handlePreviewView(Keyboard_Class::KeysState& status) {
  // Handle preview view controls - ESC or V to exit, 1/2/3 to change background
  if (M5Cardputer.Keyboard.isPressed()) {
    // Check for character keys
    for (auto i : status.word) {
      // ` key (ESC) or V key - exit preview view
      if (i == '`' || i == 'v' || i == 'V') {
        exitPreviewView();
        delay(200);  // Debounce
        return;
      }
      // 1 key - Black background
      else if (i == '1') {
        previewViewBackground = 0;
        enterPreviewView();  // Redraw with new background
        delay(150);  // Debounce
      }
      // 2 key - White background
      else if (i == '2') {
        previewViewBackground = 1;
        enterPreviewView();  // Redraw with new background
        delay(150);  // Debounce
      }
      // 3 key - Light gray background
      else if (i == '3') {
        previewViewBackground = 2;
        enterPreviewView();  // Redraw with new background
        delay(150);  // Debounce
      }
      // 4 key - Dark gray background
      else if (i == '4') {
        previewViewBackground = 3;
        enterPreviewView();  // Redraw with new background
        delay(150);  // Debounce
      }
#if ENABLE_SCREENSHOTS
      // Y key - Take Screenshot
      else if (i == 'y' || i == 'Y') {
        takeScreenshot();
        enterPreviewView();  // Redraw preview view after screenshot status message
      }
#endif
    }
  }

  delay(10);
}

/**
 * Handle Palette View input and rendering
 */
void handlePaletteView(Keyboard_Class::KeysState& status) {
  // Handle palette view controls
  static bool paletteViewNeedsRedraw = true;
  static int lastPaletteViewCursor = -1;

  // Handle insertion animation
  if (paletteInsertionAnimating) {
    // Update animation progress
    paletteInsertionProgress += PALETTE_INSERT_SPEED;

    if (paletteInsertionProgress >= 1.0f) {
      // Animation complete - clamp to 1.0
      paletteInsertionProgress = 1.0f;

      // Redraw final frame
      drawPaletteView(false);

      // Hold the final frame longer so user can see the insertion
      delay(500);

      // Now exit
      paletteInsertionAnimating = false;
      exitPaletteView();
      paletteViewNeedsRedraw = true;
      lastPaletteViewCursor = -1;
      return;
    }

    // Redraw with animation (use canvas, don't clear title)
    drawPaletteView(false);
  }
  // Redraw when cursor changes, first time, OR when animation is in progress
  else if (paletteViewNeedsRedraw || lastPaletteViewCursor != paletteViewCursor) {
    drawPaletteView(true);
    paletteViewNeedsRedraw = false;
    lastPaletteViewCursor = paletteViewCursor;
  }
  // Also redraw if scroll animation is still happening (smooth scrolling)
  // Throttle to ~30fps to prevent slowdown
  else if (fabs(paletteViewScrollPos - (float)paletteViewCursor) > 0.01f) {
    unsigned long now = millis();
    if (now - lastPaletteAnimTime >= PALETTE_ANIM_FRAME_MS) {
      drawPaletteView(false);  // Don't clear title, just update content
      lastPaletteAnimTime = now;
    }
  }

  if (M5Cardputer.Keyboard.isPressed()) {
    // Enter key - select palette and apply to active sketch
    if (status.enter) {
      // Freeze current position WITHOUT snapping (prevents any pixel shift)
      // All cartridges will use this exact position during animation
      paletteInsertionFrozenScrollPos = paletteViewScrollPos;

      // Start insertion animation
      paletteInsertionAnimating = true;
      paletteInsertionProgress = 0.0f;

      // Get actual palette index from filtered list
      uint8_t selectedPaletteIdx = filteredPaletteIndices[paletteViewCursor];

      // Copy selected palette to active sketch
      activeSketch.paletteSize = allPaletteSizes[selectedPaletteIdx];  // Set palette size
      for (int i = 0; i < 16; i++) {
        activeSketch.paletteColors[i] = pgm_read_word(&allPalettes[selectedPaletteIdx][i]);
      }

      // Update LED matrix with new palette colors
      LED_CANVAS_UPDATED();
    }

    // Check for character keys
    for (auto i : status.word) {
      // ` key (ESC) or P key - exit palette view
      if (i == '`' || i == 'p' || i == 'P') {
        exitPaletteView();
        paletteViewNeedsRedraw = true;
        lastPaletteViewCursor = -1;
        delay(200);  // Debounce
        return;
      }
      // Filter keys
      else if (i == '0') {
        // Clear all filters
        paletteFilterSize = 0;
        paletteFilterUser = false;
        updatePaletteFilter();
        paletteViewCursor = 0;
        paletteViewScrollPos = 0;
        paletteViewNeedsRedraw = true;
        // Wait for key release to prevent multiple toggles
        while (M5Cardputer.Keyboard.isPressed()) {
          M5Cardputer.update();
          delay(10);
        }
        delay(50);  // Extra debounce
        break;
      }
      else if (i == '4') {
        // Toggle 4-color filter
        paletteFilterSize = (paletteFilterSize == 4) ? 0 : 4;
        updatePaletteFilter();
        if (paletteViewCursor >= filteredPaletteCount) {
          paletteViewCursor = 0;
          paletteViewScrollPos = 0;
        }
        paletteViewNeedsRedraw = true;
        // Wait for key release to prevent multiple toggles
        while (M5Cardputer.Keyboard.isPressed()) {
          M5Cardputer.update();
          delay(10);
        }
        delay(50);  // Extra debounce
        break;
      }
      else if (i == '8') {
        // Toggle 8-color filter
        paletteFilterSize = (paletteFilterSize == 8) ? 0 : 8;
        updatePaletteFilter();
        if (paletteViewCursor >= filteredPaletteCount) {
          paletteViewCursor = 0;
          paletteViewScrollPos = 0;
        }
        paletteViewNeedsRedraw = true;
        // Wait for key release to prevent multiple toggles
        while (M5Cardputer.Keyboard.isPressed()) {
          M5Cardputer.update();
          delay(10);
        }
        delay(50);  // Extra debounce
        break;
      }
      else if (i == '1') {
        // Toggle 16-color filter
        paletteFilterSize = (paletteFilterSize == 16) ? 0 : 16;
        updatePaletteFilter();
        if (paletteViewCursor >= filteredPaletteCount) {
          paletteViewCursor = 0;
          paletteViewScrollPos = 0;
        }
        paletteViewNeedsRedraw = true;
        // Wait for key release to prevent multiple toggles
        while (M5Cardputer.Keyboard.isPressed()) {
          M5Cardputer.update();
          delay(10);
        }
        delay(50);  // Extra debounce
        break;
      }
      else if (i == 'u' || i == 'U') {
        // Toggle user palette filter
        paletteFilterUser = !paletteFilterUser;
        updatePaletteFilter();
        if (paletteViewCursor >= filteredPaletteCount) {
          paletteViewCursor = 0;
          paletteViewScrollPos = 0;
        }
        paletteViewNeedsRedraw = true;
        // Wait for key release to prevent multiple toggles
        while (M5Cardputer.Keyboard.isPressed()) {
          M5Cardputer.update();
          delay(10);
        }
        delay(50);  // Extra debounce
        break;
      }
      // Left arrow - previous palette
      else if (i == ',' && paletteViewCursor > 0) {
        paletteViewCursor--;
        delay(150);
      }
      // Right arrow - next palette
      else if (i == '/' && paletteViewCursor < filteredPaletteCount - 1) {
        paletteViewCursor++;
        delay(150);
      }
#if ENABLE_SCREENSHOTS
      // Y key - Take Screenshot
      else if (i == 'y' || i == 'Y') {
        takeScreenshot();
        paletteViewNeedsRedraw = true;  // Redraw palette view after screenshot status message
      }
#endif
    }
  }

  delay(10);
}

/**
 * Handle Canvas View input and rendering
 */
void handleCanvasView(Keyboard_Class::KeysState& status);

// loop() runs over and over again forever
void loop() {
  // Update the M5 hardware state (this checks for keyboard input)
  M5Cardputer.update();

  // Get current keyboard state
  Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

  // ============================================================================
  // HELP VIEW
  // ============================================================================
  if (inHelpView) {
    handleHelpView(status);
    return;
  }

  // ============================================================================
  // MEMORY VIEW
  // ============================================================================
  if (inMemoryView) {
    handleMemoryView(status);
    return;
  }

  // ============================================================================
  // PREVIEW VIEW
  // ============================================================================
  if (inPreviewView) {
    handlePreviewView(status);
    return;
  }

  // ============================================================================
  // PALETTE VIEW
  // ============================================================================
  if (inPaletteView) {
    handlePaletteView(status);
    return;
  }

  // ============================================================================
  // CANVAS VIEW (Default)
  // ============================================================================
  handleCanvasView(status);
}

void handleCanvasView(Keyboard_Class::KeysState& status) {
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
  int oldX = cursorX;
  int oldY = cursorY;

  // Check if enter or delete is currently being held (for drawing while moving)
  bool enterHeld = false;
  bool deleteHeld = false;

  // Check for G0 button (physical button on the device)
  // The G0 button is accessed via M5Cardputer.BtnA
  if (M5Cardputer.BtnA.wasPressed()) {
    clearCanvas();
    canvasCleared = true;
    LED_CANVAS_UPDATED();  // Update LED matrix
  }

  // Check if enter or delete are currently held
  enterHeld = status.enter;
  deleteHeld = status.del;

  // Check if any key was pressed (for non-repeating actions)
  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      // Check for special keys first (Enter, Backspace, etc.)
      // These are in status.enter, status.del, etc., not in status.word
      if (status.enter) {
        // Enter/Return key (OK button) - place pixel with selected color
        saveUndo();  // Save state before placing pixel
        canvas[cursorY][cursorX] = selectedColor;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();  // Update LED matrix
      }
      else if (status.del) {
        // Backspace/Delete key - erase pixel
        saveUndo();  // Save state before erasing
        canvas[cursorY][cursorX] = 0;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();  // Update LED matrix
      }

      // Check for non-arrow keys (number keys, commands, etc.)
      // The Cardputer uses ';' for up, '.' for down, ',' for left, '/' for right
      for (auto i : status.word) {
        // Number keys 1-8 select colors
        // Without Shift: colors 1-8
        // With Shift (fn): colors 9-16 (only if palette has 16 colors)
        if (i >= '1' && i <= '8') {
          uint8_t baseColor = i - '0';  // Convert '1' to 1, '2' to 2, etc.
          uint8_t newColor = status.fn ? (baseColor + 8) : baseColor;

          // Don't allow selecting colors beyond palette size
          if (newColor <= activeSketch.paletteSize && selectedColor != newColor) {
            selectedColor = newColor;
            colorChanged = true;
            char colorMsg[20];
            snprintf(colorMsg, sizeof(colorMsg), StatusMsg::COLOR_FMT, selectedColor);
            setStatusMessage(colorMsg);
          }
        }
        // C key - Cycle to next color
        else if (i == 'c' || i == 'C') {
          selectedColor++;
          if (selectedColor > activeSketch.paletteSize) {
            selectedColor = 1;  // Loop back to first color
          }
          colorChanged = true;
          char colorMsg[20];
          snprintf(colorMsg, sizeof(colorMsg), StatusMsg::COLOR_FMT, selectedColor);
          setStatusMessage(colorMsg);
        }
        // Z key - Undo
        else if (i == 'z' || i == 'Z') {
          restoreUndo();
          undoPerformed = true;
        }
        // G key - Toggle grid size between 8×8 and 16×16
        else if (i == 'g' || i == 'G') {
          toggleGridSize();
          gridToggled = true;
        }
        // R key - Toggle rulers (center guides)
        else if (i == 'r' || i == 'R') {
          rulersVisible = !rulersVisible;
          rulersToggled = true;
          setStatusMessage(rulersVisible ? "Rulers: On" : "Rulers: Off");
        }
        // T key - Toggle theme
        else if (i == 't' || i == 'T') {
          // Toggle between light and dark themes
          if (currentTheme == &THEME_LIGHT) {
            currentTheme = &THEME_DARK;
            setStatusMessage("Dark Mode");
          } else {
            currentTheme = &THEME_LIGHT;
            setStatusMessage("Light Mode");
          }

          // Save theme preference to flash
          preferences.begin("bitmap16dx", false);
          preferences.putBool("darkMode", (currentTheme == &THEME_DARK));
          preferences.end();

          // Flag for full redraw
          themeToggled = true;
        }
        // O key - Open Memory View
        else if (i == 'o' || i == 'O') {
          enterMemoryView();
          delay(200);  // Debounce to prevent immediate close
        }
        // S key - Save sketch (or Fn+S to save as new)
        else if (i == 's' || i == 'S') {
          // Copy canvas to active sketch before saving
          for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
              activeSketch.pixels[y][x] = canvas[y][x];
            }
          }
          activeSketch.gridSize = currentGridSize;

          if (status.fn) {
            // Fn+S: Save as new sketch
            saveActiveSketchAsNew();
          } else {
            // S: Save to existing sketch (or save as new if unsaved)
            saveActiveSketchToSD();
          }
        }
        // F key - Flood fill (paint bucket)
        else if (i == 'f' || i == 'F') {
          saveUndo();  // Save state before flood fill
          floodFill(cursorX, cursorY, selectedColor);
          floodFilled = true;  // Trigger full canvas redraw
          LED_CANVAS_UPDATED();  // Update LED matrix
          setStatusMessage(StatusMsg::FILL);
          }
        // I key - Enter Hint Screen
        else if (i == 'i' || i == 'I') {
          enterHelpView();
          delay(200);  // Debounce to prevent immediate close
        }
        // V key - Enter View Mode
        else if (i == 'v' || i == 'V') {
          enterPreviewView();
          delay(200);  // Debounce to prevent immediate close
        }
        // X key - Export PNG
        // X alone = 128×128 scaled export
        // Fn+X = logical size export (8×8 or 16×16)
        else if (i == 'x' || i == 'X') {
          bool scaleToFull = !status.fn;  // Scale unless Fn is held
          exportCanvasToPNG(scaleToFull);
        }
#if ENABLE_SCREENSHOTS
        // Y key - Take Screenshot (full 240×135 display)
        else if (i == 'y' || i == 'Y') {
          takeScreenshot();
        }
#endif
        // P key - Open Palette Menu
        else if (i == 'p' || i == 'P') {
          enterPaletteView();
          delay(200);  // Debounce to prevent immediate close
        }
        // B key + Plus/Minus - Brightness control
        // Hold B and press + to increase brightness
        // Hold B and press - to decrease brightness
        else if ((i == '+' || i == '=' || i == '-') && isBKeyHeld(status)) {
          const int BRIGHTNESS_STEP = 10;  // Adjust brightness by 10% each press
          const int MIN_BRIGHTNESS = 10;   // Minimum brightness (10%)
          const int MAX_BRIGHTNESS = 100;  // Maximum brightness (100%)

          if (i == '+' || i == '=') {
            // Increase brightness by 10% (cap at 100%)
            if (displayBrightness <= MAX_BRIGHTNESS - BRIGHTNESS_STEP) {
              displayBrightness += BRIGHTNESS_STEP;
            } else {
              displayBrightness = MAX_BRIGHTNESS;
            }
          } else if (i == '-') {
            // Decrease brightness by 10% (don't go below minimum)
            if (displayBrightness > MIN_BRIGHTNESS + BRIGHTNESS_STEP) {
              displayBrightness -= BRIGHTNESS_STEP;
            } else {
              displayBrightness = MIN_BRIGHTNESS;  // Keep minimum usable brightness
            }
          }

          // Convert percentage (10-100) to hardware range (0-255)
          uint8_t hardwareBrightness = (displayBrightness * 255) / 100;

          // Apply the new brightness setting to the display
          M5Cardputer.Display.setBrightness(hardwareBrightness);

          // Save brightness setting to preferences so it persists across reboots
          preferences.begin("bitmap16dx", false);
          preferences.putUChar("brightness", displayBrightness);
          preferences.end();

          // Show brightness level as clean percentage
          char brightnessMsg[20];
          snprintf(brightnessMsg, sizeof(brightnessMsg), "BRIGHT: %d%%", displayBrightness);
          setStatusMessage(brightnessMsg);
        }
#if ENABLE_LED_MATRIX
        // L key + Enter - Toggle LED matrix on/off
        else if ((i == 'l' || i == 'L') && status.enter) {
          toggleLEDMatrix();
          char ledMsg[30];
          snprintf(ledMsg, sizeof(ledMsg), "LED: %s", ledMatrixEnabled ? "ON" : "OFF");
          setStatusMessage(ledMsg);
        }
        // L key + Plus/Minus - LED matrix brightness control
        // Hold L and press + to increase brightness
        // Hold L and press - to decrease brightness
        else if ((i == '+' || i == '=' || i == '-') && isLKeyHeld(status)) {
          adjustLEDBrightness((i == '-') ? -5 : +5);

          // Show LED matrix brightness level
          char ledBrightMsg[30];
          snprintf(ledBrightMsg, sizeof(ledBrightMsg), "LED: %d%%", ledBrightness);
          setStatusMessage(ledBrightMsg);
        }
#endif // ENABLE_LED_MATRIX
        // Arrow keys - handle first press
        else if (i == ';' || i == '.' || i == ',' || i == '/') {
          lastKey = i;
          lastKeyTime = millis();
          keyRepeating = false;
          // Process the initial keypress
          if (i == ';' && cursorY > 0) {
            cursorY--;
            moved = true;
          }
          else if (i == '.' && cursorY < currentGridSize - 1) {
            cursorY++;
            moved = true;
          }
          else if (i == ',' && cursorX > 0) {
            cursorX--;
            moved = true;
          }
          else if (i == '/' && cursorX < currentGridSize - 1) {
            cursorX++;
            moved = true;
          }

          // If enter or delete is held, paint/erase at the new position
          if (moved && enterHeld) {
            canvas[cursorY][cursorX] = selectedColor;
            pixelPlaced = true;
            LED_CANVAS_UPDATED();  // Update LED matrix
              }
          else if (moved && deleteHeld) {
            canvas[cursorY][cursorX] = 0;
            pixelPlaced = true;
            LED_CANVAS_UPDATED();  // Update LED matrix
              }
        }
      }
    }
  }

  // Handle key repeat for arrow keys when held
  // Check if an arrow key is currently being held
  bool arrowKeyHeld = false;
  char currentArrowKey = 0;

  for (auto i : status.word) {
    if (i == ';' || i == '.' || i == ',' || i == '/') {
      arrowKeyHeld = true;
      currentArrowKey = i;
      break;
    }
  }

  if (arrowKeyHeld && currentArrowKey == lastKey) {
    // Key is still held - check if we should repeat
    unsigned long currentTime = millis();
    unsigned long timeSinceLastAction = currentTime - lastKeyTime;

    // Determine the threshold based on whether we're in repeat mode
    unsigned long threshold = keyRepeating ? keyRepeatRate : keyRepeatDelay;

    if (timeSinceLastAction >= threshold) {
      // Time to repeat!
      keyRepeating = true;
      lastKeyTime = currentTime;

      // Process the arrow key movement
      if (currentArrowKey == ';' && cursorY > 0) {
        cursorY--;
        moved = true;
      }
      else if (currentArrowKey == '.' && cursorY < currentGridSize - 1) {
        cursorY++;
        moved = true;
      }
      else if (currentArrowKey == ',' && cursorX > 0) {
        cursorX--;
        moved = true;
      }
      else if (currentArrowKey == '/' && cursorX < currentGridSize - 1) {
        cursorX++;
        moved = true;
      }

      // If enter or delete is held, paint/erase at the new position
      if (moved && enterHeld) {
        canvas[cursorY][cursorX] = selectedColor;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();  // Update LED matrix
      }
      else if (moved && deleteHeld) {
        canvas[cursorY][cursorX] = 0;
        pixelPlaced = true;
        LED_CANVAS_UPDATED();  // Update LED matrix
      }
    }
  } else {
    // No arrow key held - reset repeat state
    lastKey = 0;
    keyRepeating = false;
  }

  // Update LED matrix when cursor moves (to highlight current position)
  if (moved) {
    LED_CANVAS_UPDATED();
  }

  // Redraw based on what changed
  if (canvasCleared || undoPerformed || gridToggled || rulersToggled || floodFilled || themeToggled) {
    // Redraw the entire canvas
    // (gridToggled needs full redraw because cell size changed)
    // (rulersToggled needs full redraw to show/hide rulers)
    // (floodFilled needs full redraw because many cells may have changed)
    // (themeToggled needs full redraw with new background color)

    // If theme changed, clear entire screen with new background
    if (themeToggled) {
      M5Cardputer.Display.fillScreen(currentTheme->background);

      // Redraw all UI elements
      drawGrid();
      drawPalette();
      drawCursor();

      // Redraw icons
      drawIcon(3, 3, ICON_DRAW, ICON_DRAW_WIDTH, ICON_DRAW_HEIGHT, ICON_DRAW_IS_INDEXED);
      drawIcon(3, 30, ICON_ERASE, ICON_ERASE_WIDTH, ICON_ERASE_HEIGHT, ICON_ERASE_IS_INDEXED);
      drawIcon(3, 57, ICON_FILL, ICON_FILL_WIDTH, ICON_FILL_HEIGHT, ICON_FILL_IS_INDEXED);

      // Reset battery indicator for redraw
      lastBatteryPercent = -1;
      batteryFirstCheck = true;
      drawBatteryIndicator();
    } else {
      drawGrid();
      for (int y = 0; y < currentGridSize; y++) {
        for (int x = 0; x < currentGridSize; x++) {
          if (canvas[y][x] != 0) {
            drawCell(x, y);
          }
        }
      }

      drawCursor();
    }
  }
  else if (moved) {
    // Erase the old cursor by redrawing that cell
    drawCell(oldX, oldY);

    // Draw the new cursor
    drawCursor();
  }
  else if (pixelPlaced) {
    // Redraw the current cell and cursor
    drawCell(cursorX, cursorY);
    drawCursor();
  }
  else if (colorChanged) {
    // Redraw the entire palette with new selection
    drawPalette();
    // Also redraw the cursor to show the new color preview
    drawCursor();
  }

  // Check if status message needs to be cleared (independent of other redraws)
  if (statusMessageJustCleared) {
    // Status message starts at x=3, y=124 and can extend ~110px
    // It overlaps background (x=3-55) AND grid (x=56+)

    // Clear the background area (left of grid)
    M5Cardputer.Display.fillRect(3, 124, 53, 11, currentTheme->background);

    // Redraw bottom grid rows that text might have overlapped
    int affectedStartY = 124 - GRID_Y;  // 120
    int cellSize = 128 / currentGridSize;
    int startRow = affectedStartY / cellSize;

    for (int y = startRow; y < currentGridSize; y++) {
      for (int x = 0; x < currentGridSize; x++) {
        drawCell(x, y);
      }
    }

    drawCursor();
    statusMessageJustCleared = false;

    // Draw current message (if any)
    if (statusMessage[0] != '\0') {
      M5Cardputer.Display.setTextColor(currentTheme->text);
      M5Cardputer.Display.setTextSize(1);
      M5Cardputer.Display.setCursor(3, 124);
      M5Cardputer.Display.print(statusMessage);
    }
  }

  // Always update status message display (check for expiration and trigger redraws)
  drawStatusMessage();

  // Update battery indicator (only redraws if changed)
  drawBatteryIndicator();

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
