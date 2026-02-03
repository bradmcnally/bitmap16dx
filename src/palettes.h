/**
 * palettes.h
 *
 * Color palette definitions for BitMap16 DX
 * All palettes are stored in PROGMEM (flash memory)
 */

#ifndef PALETTES_H
#define PALETTES_H

#include <Arduino.h>

// Helper macro to convert RGB888 (hex color #RRGGBB) to RGB565
// Usage: RGB565(0x32, 0x00, 0x11) for #320011
#define RGB565(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

// ============================================================================
// PALETTE DEFINITIONS
// ============================================================================
// Multiple palettes that can be swapped between
// Index 0 is reserved for transparent/empty cells

// Palette 0: SWEETIE-16 (default)
const uint16_t PALETTE_SWEETIE16[16] PROGMEM = {
  RGB565(0x1A, 0x1C, 0x2C),  // 1. #1A1C2C - Very dark blue-gray
  RGB565(0x5D, 0x27, 0x5D),  // 2. #5D275D - Dark purple
  RGB565(0xB1, 0x3E, 0x53),  // 3. #B13E53 - Dark red
  RGB565(0xEF, 0x7D, 0x57),  // 4. #EF7D57 - Orange
  RGB565(0xFF, 0xCD, 0x75),  // 5. #FFCD75 - Light orange
  RGB565(0xA7, 0xF0, 0x70),  // 6. #A7F070 - Light green
  RGB565(0x38, 0xB7, 0x64),  // 7. #38B764 - Green
  RGB565(0x25, 0x71, 0x79),  // 8. #257179 - Teal
  RGB565(0x29, 0x36, 0x6F),  // 9. #29366F - Dark blue
  RGB565(0x3B, 0x5D, 0xC9),  // 10. #3B5DC9 - Blue
  RGB565(0x41, 0xA6, 0xF6),  // 11. #41A6F6 - Light blue
  RGB565(0x73, 0xEF, 0xF7),  // 12. #73EFF7 - Cyan
  RGB565(0xF4, 0xF4, 0xF4),  // 13. #F4F4F4 - White
  RGB565(0x94, 0xB0, 0xC2),  // 14. #94B0C2 - Light gray-blue
  RGB565(0x56, 0x6C, 0x86),  // 15. #566C86 - Gray-blue
  RGB565(0x33, 0x3C, 0x57)   // 16. #333C57 - Dark gray-blue
};

// Palette 1: PICO-8
const uint16_t PALETTE_PICO8[16] PROGMEM = {
  RGB565(0x00, 0x00, 0x00),  // 1. Black
  RGB565(0x1D, 0x2B, 0x53),  // 2. Dark blue
  RGB565(0x7E, 0x25, 0x53),  // 3. Dark purple
  RGB565(0x00, 0x87, 0x51),  // 4. Dark green
  RGB565(0xAB, 0x52, 0x36),  // 5. Brown
  RGB565(0x5F, 0x57, 0x4F),  // 6. Dark gray
  RGB565(0xC2, 0xC3, 0xC7),  // 7. Light gray
  RGB565(0xFF, 0xF1, 0xE8),  // 8. White
  RGB565(0xFF, 0x00, 0x4D),  // 9. Red
  RGB565(0xFF, 0xA3, 0x00),  // 10. Orange
  RGB565(0xFF, 0xEC, 0x27),  // 11. Yellow
  RGB565(0x00, 0xE4, 0x36),  // 12. Green
  RGB565(0x29, 0xAD, 0xFF),  // 13. Blue
  RGB565(0x83, 0x76, 0x9C),  // 14. Indigo
  RGB565(0xFF, 0x77, 0xA8),  // 15. Pink
  RGB565(0xFF, 0xCC, 0xAA)   // 16. Peach
};

// Palette 2: NA16 (retro 16-color palette)
const uint16_t PALETTE_NA16[16] PROGMEM = {
  RGB565(0x8C, 0x8F, 0xAE),  // 1. #8C8FAE - Light blue-gray
  RGB565(0x58, 0x45, 0x63),  // 2. #584563 - Dark purple
  RGB565(0x3E, 0x21, 0x37),  // 3. #3E2137 - Very dark purple
  RGB565(0x9A, 0x63, 0x48),  // 4. #9A6348 - Brown
  RGB565(0xD7, 0x9B, 0x7D),  // 5. #D79B7D - Light brown
  RGB565(0xF5, 0xED, 0xBA),  // 6. #F5EDBA - Cream
  RGB565(0xC0, 0xC7, 0x41),  // 7. #C0C741 - Yellow-green
  RGB565(0x64, 0x7D, 0x34),  // 8. #647D34 - Olive
  RGB565(0xE4, 0x94, 0x3A),  // 9. #E4943A - Orange
  RGB565(0x9D, 0x30, 0x3B),  // 10. #9D303B - Dark red
  RGB565(0xD2, 0x64, 0x71),  // 11. #D26471 - Pink-red
  RGB565(0x70, 0x37, 0x7F),  // 12. #70377F - Purple
  RGB565(0x7E, 0xC4, 0xC1),  // 13. #7EC4C1 - Cyan
  RGB565(0x34, 0x85, 0x9D),  // 14. #34859D - Blue
  RGB565(0x17, 0x43, 0x4B),  // 15. #17434B - Dark teal
  RGB565(0x1F, 0x0E, 0x1C)   // 16. #1F0E1C - Almost black
};

// Palette 3: WOODSPARK (16-color nature palette)
const uint16_t PALETTE_WOODSPARK[16] PROGMEM = {
  RGB565(0xF5, 0xEE, 0xB0),  // 1. #F5EEB0 - Cream
  RGB565(0xFA, 0xBF, 0x61),  // 2. #FABF61 - Light orange
  RGB565(0xE0, 0x8D, 0x51),  // 3. #E08D51 - Orange
  RGB565(0x8A, 0x58, 0x65),  // 4. #8A5865 - Mauve
  RGB565(0x45, 0x2B, 0x3F),  // 5. #452B3F - Dark purple
  RGB565(0x2C, 0x5E, 0x3B),  // 6. #2C5E3B - Dark green
  RGB565(0x60, 0x9C, 0x4F),  // 7. #609C4F - Green
  RGB565(0xC6, 0xCC, 0x54),  // 8. #C6CC54 - Yellow-green
  RGB565(0x78, 0xC2, 0xD6),  // 9. #78C2D6 - Light blue
  RGB565(0x54, 0x79, 0xB0),  // 10. #5479B0 - Blue
  RGB565(0x56, 0x54, 0x6E),  // 11. #56546E - Dark gray
  RGB565(0x83, 0x9F, 0xA6),  // 12. #839FA6 - Blue-gray
  RGB565(0xE0, 0xD3, 0xC8),  // 13. #E0D3C8 - Tan
  RGB565(0xF0, 0x5B, 0x5B),  // 14. #F05B5B - Red
  RGB565(0x8F, 0x32, 0x5F),  // 15. #8F325F - Dark pink
  RGB565(0xEB, 0x6C, 0x98)   // 16. #EB6C98 - Pink
};

// Palette 4: GOTHIC BIT (8-color gray/purple tones)
const uint16_t PALETTE_GOTHICBIT[16] PROGMEM = {
  RGB565(0x0E, 0x0E, 0x12),  // 1. #0E0E12 - Almost black
  RGB565(0x1A, 0x1A, 0x24),  // 2. #1A1A24 - Very dark purple-gray
  RGB565(0x33, 0x33, 0x46),  // 3. #333346 - Dark purple-gray
  RGB565(0x53, 0x53, 0x73),  // 4. #535373 - Medium purple-gray
  RGB565(0x80, 0x80, 0xA4),  // 5. #8080A4 - Light purple-gray
  RGB565(0xA6, 0xA6, 0xBF),  // 6. #A6A6BF - Lighter purple-gray
  RGB565(0xC1, 0xC1, 0xD2),  // 7. #C1C1D2 - Very light purple-gray
  RGB565(0xE6, 0xE6, 0xEC),  // 8. #E6E6EC - Almost white
  // Indices 9-16 repeat the 8 colors
  RGB565(0x0E, 0x0E, 0x12),  // 9. Maps to color 1
  RGB565(0x1A, 0x1A, 0x24),  // 10. Maps to color 2
  RGB565(0x33, 0x33, 0x46),  // 11. Maps to color 3
  RGB565(0x53, 0x53, 0x73),  // 12. Maps to color 4
  RGB565(0x80, 0x80, 0xA4),  // 13. Maps to color 5
  RGB565(0xA6, 0xA6, 0xBF),  // 14. Maps to color 6
  RGB565(0xC1, 0xC1, 0xD2),  // 15. Maps to color 7
  RGB565(0xE6, 0xE6, 0xEC)   // 16. Maps to color 8
};

// Palette 5: DREAM HAZE (8-color purple/pink gradient)
const uint16_t PALETTE_DREAMHAZE[16] PROGMEM = {
  RGB565(0x3C, 0x42, 0xC4),  // 1. #3C42C4 - Blue
  RGB565(0x6E, 0x51, 0xC8),  // 2. #6E51C8 - Purple-blue
  RGB565(0xA0, 0x65, 0xCD),  // 3. #A065CD - Purple
  RGB565(0xCE, 0x79, 0xD2),  // 4. #CE79D2 - Pink-purple
  RGB565(0xD6, 0x8F, 0xB8),  // 5. #D68FB8 - Pink
  RGB565(0xDD, 0xA2, 0xA3),  // 6. #DDA2A3 - Light pink
  RGB565(0xEA, 0xC4, 0xAE),  // 7. #EAC4AE - Peach
  RGB565(0xF4, 0xDF, 0xBE),  // 8. #F4DFBE - Cream
  // Indices 9-16 repeat the 8 colors
  RGB565(0x3C, 0x42, 0xC4),  // 9. Maps to color 1
  RGB565(0x6E, 0x51, 0xC8),  // 10. Maps to color 2
  RGB565(0xA0, 0x65, 0xCD),  // 11. Maps to color 3
  RGB565(0xCE, 0x79, 0xD2),  // 12. Maps to color 4
  RGB565(0xD6, 0x8F, 0xB8),  // 13. Maps to color 5
  RGB565(0xDD, 0xA2, 0xA3),  // 14. Maps to color 6
  RGB565(0xEA, 0xC4, 0xAE),  // 15. Maps to color 7
  RGB565(0xF4, 0xDF, 0xBE)   // 16. Maps to color 8
};

// Palette 6: POLLEN8 (8-color warm/cool gradient)
const uint16_t PALETTE_POLLEN8[16] PROGMEM = {
  RGB565(0x73, 0x46, 0x4C),  // 1. #73464C - Dark mauve
  RGB565(0xAB, 0x56, 0x75),  // 2. #AB5675 - Mauve
  RGB565(0xEE, 0x6A, 0x7C),  // 3. #EE6A7C - Pink
  RGB565(0xFF, 0xA7, 0xA5),  // 4. #FFA7A5 - Light pink
  RGB565(0xFF, 0xE0, 0x7E),  // 5. #FFE07E - Yellow
  RGB565(0xFF, 0xE7, 0xD6),  // 6. #FFE7D6 - Cream
  RGB565(0x72, 0xDC, 0xBB),  // 7. #72DCBB - Turquoise
  RGB565(0x34, 0xAC, 0xBA),  // 8. #34ACBA - Teal
  // Indices 9-16 repeat the 8 colors
  RGB565(0x73, 0x46, 0x4C),  // 9. Maps to color 1
  RGB565(0xAB, 0x56, 0x75),  // 10. Maps to color 2
  RGB565(0xEE, 0x6A, 0x7C),  // 11. Maps to color 3
  RGB565(0xFF, 0xA7, 0xA5),  // 12. Maps to color 4
  RGB565(0xFF, 0xE0, 0x7E),  // 13. Maps to color 5
  RGB565(0xFF, 0xE7, 0xD6),  // 14. Maps to color 6
  RGB565(0x72, 0xDC, 0xBB),  // 15. Maps to color 7
  RGB565(0x34, 0xAC, 0xBA)   // 16. Maps to color 8
};

// Palette 7: FUNKYFUTURE-8 (8-color vibrant gradient)
const uint16_t PALETTE_FUNKYFUTURE8[16] PROGMEM = {
  RGB565(0x2B, 0x0F, 0x54),  // 1. #2B0F54 - Dark purple
  RGB565(0xAB, 0x1F, 0x65),  // 2. #AB1F65 - Magenta
  RGB565(0xFF, 0x4F, 0x69),  // 3. #FF4F69 - Pink-red
  RGB565(0xFF, 0xF7, 0xF8),  // 4. #FFF7F8 - White
  RGB565(0xFF, 0x81, 0x42),  // 5. #FF8142 - Orange
  RGB565(0xFF, 0xDA, 0x45),  // 6. #FFDA45 - Yellow
  RGB565(0x33, 0x68, 0xDC),  // 7. #3368DC - Blue
  RGB565(0x49, 0xE7, 0xEC),  // 8. #49E7EC - Cyan
  // Indices 9-16 repeat the 8 colors
  RGB565(0x2B, 0x0F, 0x54),  // 9. Maps to color 1
  RGB565(0xAB, 0x1F, 0x65),  // 10. Maps to color 2
  RGB565(0xFF, 0x4F, 0x69),  // 11. Maps to color 3
  RGB565(0xFF, 0xF7, 0xF8),  // 12. Maps to color 4
  RGB565(0xFF, 0x81, 0x42),  // 13. Maps to color 5
  RGB565(0xFF, 0xDA, 0x45),  // 14. Maps to color 6
  RGB565(0x33, 0x68, 0xDC),  // 15. Maps to color 7
  RGB565(0x49, 0xE7, 0xEC)   // 16. Maps to color 8
};

// Palette 8: ICE CREAM GB (4-color pastel palette)
const uint16_t PALETTE_ICECREAMGB[16] PROGMEM = {
  RGB565(0x7C, 0x3F, 0x58),  // 1. #7C3F58 - Dark purple
  RGB565(0xEB, 0x6B, 0x6F),  // 2. #EB6B6F - Coral red
  RGB565(0xF9, 0xA8, 0x75),  // 3. #F9A875 - Peach
  RGB565(0xFF, 0xF6, 0xD3),  // 4. #FFF6D3 - Cream
  // Repeat for compatibility with 16-color system
  RGB565(0x7C, 0x3F, 0x58),  // 5. Maps to color 1
  RGB565(0xEB, 0x6B, 0x6F),  // 6. Maps to color 2
  RGB565(0xF9, 0xA8, 0x75),  // 7. Maps to color 3
  RGB565(0xFF, 0xF6, 0xD3),  // 8. Maps to color 4
  RGB565(0x7C, 0x3F, 0x58),  // 9. Maps to color 1
  RGB565(0xEB, 0x6B, 0x6F),  // 10. Maps to color 2
  RGB565(0xF9, 0xA8, 0x75),  // 11. Maps to color 3
  RGB565(0xFF, 0xF6, 0xD3),  // 12. Maps to color 4
  RGB565(0x7C, 0x3F, 0x58),  // 13. Maps to color 1
  RGB565(0xEB, 0x6B, 0x6F),  // 14. Maps to color 2
  RGB565(0xF9, 0xA8, 0x75),  // 15. Maps to color 3
  RGB565(0xFF, 0xF6, 0xD3)   // 16. Maps to color 4
};

// Palette 9: HOLLOW (4-color minimal palette)
const uint16_t PALETTE_HOLLOW[16] PROGMEM = {
  RGB565(0x0F, 0x0F, 0x1B),  // 1. #0F0F1B - Very dark blue
  RGB565(0x56, 0x5A, 0x75),  // 2. #565A75 - Blue-gray
  RGB565(0xC6, 0xB7, 0xBE),  // 3. #C6B7BE - Light purple-gray
  RGB565(0xFA, 0xFB, 0xF6),  // 4. #FAFBF6 - Off-white
  // Repeat for compatibility with 16-color system
  RGB565(0x0F, 0x0F, 0x1B),  // 5. Maps to color 1
  RGB565(0x56, 0x5A, 0x75),  // 6. Maps to color 2
  RGB565(0xC6, 0xB7, 0xBE),  // 7. Maps to color 3
  RGB565(0xFA, 0xFB, 0xF6),  // 8. Maps to color 4
  RGB565(0x0F, 0x0F, 0x1B),  // 9. Maps to color 1
  RGB565(0x56, 0x5A, 0x75),  // 10. Maps to color 2
  RGB565(0xC6, 0xB7, 0xBE),  // 11. Maps to color 3
  RGB565(0xFA, 0xFB, 0xF6),  // 12. Maps to color 4
  RGB565(0x0F, 0x0F, 0x1B),  // 13. Maps to color 1
  RGB565(0x56, 0x5A, 0x75),  // 14. Maps to color 2
  RGB565(0xC6, 0xB7, 0xBE),  // 15. Maps to color 3
  RGB565(0xFA, 0xFB, 0xF6)   // 16. Maps to color 4
};

// Palette 10: KIROKAZE GAMEBOY (4-color Game Boy palette)
const uint16_t PALETTE_KIROKAZEGB[16] PROGMEM = {
  RGB565(0x33, 0x2C, 0x50),  // 1. #332C50 - Dark purple
  RGB565(0x46, 0x87, 0x8F),  // 2. #46878F - Teal
  RGB565(0x94, 0xE3, 0x44),  // 3. #94E344 - Lime green
  RGB565(0xE2, 0xF3, 0xE4),  // 4. #E2F3E4 - Light mint
  // Repeat for compatibility with 16-color system
  RGB565(0x33, 0x2C, 0x50),  // 5. Maps to color 1
  RGB565(0x46, 0x87, 0x8F),  // 6. Maps to color 2
  RGB565(0x94, 0xE3, 0x44),  // 7. Maps to color 3
  RGB565(0xE2, 0xF3, 0xE4),  // 8. Maps to color 4
  RGB565(0x33, 0x2C, 0x50),  // 9. Maps to color 1
  RGB565(0x46, 0x87, 0x8F),  // 10. Maps to color 2
  RGB565(0x94, 0xE3, 0x44),  // 11. Maps to color 3
  RGB565(0xE2, 0xF3, 0xE4),  // 12. Maps to color 4
  RGB565(0x33, 0x2C, 0x50),  // 13. Maps to color 1
  RGB565(0x46, 0x87, 0x8F),  // 14. Maps to color 2
  RGB565(0x94, 0xE3, 0x44),  // 15. Maps to color 3
  RGB565(0xE2, 0xF3, 0xE4)   // 16. Maps to color 4
};

// Palette 11: LAVA GB (4-color hot gradient)
const uint16_t PALETTE_LAVAGB[16] PROGMEM = {
  RGB565(0x05, 0x1F, 0x39),  // 1. #051F39 - Dark blue
  RGB565(0x4A, 0x24, 0x80),  // 2. #4A2480 - Purple
  RGB565(0xC5, 0x3A, 0x9D),  // 3. #C53A9D - Magenta
  RGB565(0xFF, 0x8E, 0x80),  // 4. #FF8E80 - Coral
  // Repeat for compatibility with 16-color system
  RGB565(0x05, 0x1F, 0x39),  // 5. Maps to color 1
  RGB565(0x4A, 0x24, 0x80),  // 6. Maps to color 2
  RGB565(0xC5, 0x3A, 0x9D),  // 7. Maps to color 3
  RGB565(0xFF, 0x8E, 0x80),  // 8. Maps to color 4
  RGB565(0x05, 0x1F, 0x39),  // 9. Maps to color 1
  RGB565(0x4A, 0x24, 0x80),  // 10. Maps to color 2
  RGB565(0xC5, 0x3A, 0x9D),  // 11. Maps to color 3
  RGB565(0xFF, 0x8E, 0x80),  // 12. Maps to color 4
  RGB565(0x05, 0x1F, 0x39),  // 13. Maps to color 1
  RGB565(0x4A, 0x24, 0x80),  // 14. Maps to color 2
  RGB565(0xC5, 0x3A, 0x9D),  // 15. Maps to color 3
  RGB565(0xFF, 0x8E, 0x80)   // 16. Maps to color 4
};

// Default palette (used for new slots)
const uint16_t PALETTE[16] = {
  RGB565(0x1A, 0x1C, 0x2C),  // 1. #1A1C2C - Very dark blue-gray
  RGB565(0x5D, 0x27, 0x5D),  // 2. #5D275D - Dark purple
  RGB565(0xB1, 0x3E, 0x53),  // 3. #B13E53 - Dark red
  RGB565(0xEF, 0x7D, 0x57),  // 4. #EF7D57 - Orange
  RGB565(0xFF, 0xCD, 0x75),  // 5. #FFCD75 - Light orange
  RGB565(0xA7, 0xF0, 0x70),  // 6. #A7F070 - Light green
  RGB565(0x38, 0xB7, 0x64),  // 7. #38B764 - Green
  RGB565(0x25, 0x71, 0x79),  // 8. #257179 - Teal
  RGB565(0x29, 0x36, 0x6F),  // 9. #29366F - Dark blue
  RGB565(0x3B, 0x5D, 0xC9),  // 10. #3B5DC9 - Blue
  RGB565(0x41, 0xA6, 0xF6),  // 11. #41A6F6 - Light blue
  RGB565(0x73, 0xEF, 0xF7),  // 12. #73EFF7 - Cyan
  RGB565(0xF4, 0xF4, 0xF4),  // 13. #F4F4F4 - White
  RGB565(0x94, 0xB0, 0xC2),  // 14. #94B0C2 - Light gray-blue
  RGB565(0x56, 0x6C, 0x86),  // 15. #566C86 - Gray-blue
  RGB565(0x33, 0x3C, 0x57)   // 16. #333C57 - Dark gray-blue
};

// Palette catalog - array of pointers to all available palettes
// Organized by size: 16-color, then 8-color, then 4-color
const int NUM_PALETTES = 12;
const uint16_t* PALETTE_CATALOG[NUM_PALETTES] = {
  // 16-color palettes (4 total)
  PALETTE_SWEETIE16,
  PALETTE_PICO8,
  PALETTE_NA16,
  PALETTE_WOODSPARK,
  // 8-color palettes (4 total)
  PALETTE_GOTHICBIT,
  PALETTE_DREAMHAZE,
  PALETTE_POLLEN8,
  PALETTE_FUNKYFUTURE8,
  // 4-color palettes (4 total)
  PALETTE_ICECREAMGB,
  PALETTE_HOLLOW,
  PALETTE_KIROKAZEGB,
  PALETTE_LAVAGB
};

const char* PALETTE_NAMES[NUM_PALETTES] = {
  // 16-color palettes
  "SWEETIE-16",
  "PICO-8",
  "NA16",
  "WOODSPARK",
  // 8-color palettes
  "GOTHIC BIT",
  "DREAM HAZE",
  "POLLEN8",
  "FUNKY FUTURE",
  // 4-color palettes
  "ICE CREAM",
  "HOLLOW",
  "KIROKAZE GB",
  "LAVA GB"
};

// Palette sizes (4, 8, or 16 colors)
// Organized by size: 16-color, then 8-color, then 4-color
const uint8_t PALETTE_SIZES[NUM_PALETTES] = {
  // 16-color palettes
  16,  // SWEETIE-16
  16,  // PICO-8
  16,  // NA16
  16,  // WOODSPARK
  // 8-color palettes
  8,   // GOTHIC BIT
  8,   // DREAM HAZE
  8,   // POLLEN8
  8,   // FUNKY FUTURE
  // 4-color palettes
  4,   // ICE CREAM
  4,   // HOLLOW
  4,   // KIROKAZE GB
  4    // LAVA GB
};

#endif // PALETTES_H
