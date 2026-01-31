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

// Palette 2: ENDESGA-16 (popular 16-color palette)
const uint16_t PALETTE_ENDESGA16[16] PROGMEM = {
  RGB565(0xE4, 0xA6, 0x72),  // 1. #E4A672 - Peach
  RGB565(0xB8, 0x6F, 0x50),  // 2. #B86F50 - Brown
  RGB565(0x74, 0x3F, 0x39),  // 3. #743F39 - Dark brown
  RGB565(0x3F, 0x28, 0x32),  // 4. #3F2832 - Very dark brown
  RGB565(0x9E, 0x28, 0x35),  // 5. #9E2835 - Dark red
  RGB565(0xE5, 0x3B, 0x44),  // 6. #E53B44 - Red
  RGB565(0xFB, 0x92, 0x2B),  // 7. #FB922B - Orange
  RGB565(0xFF, 0xE7, 0x62),  // 8. #FFE762 - Yellow
  RGB565(0x63, 0xC6, 0x4D),  // 9. #63C64D - Green
  RGB565(0x32, 0x73, 0x45),  // 10. #327345 - Dark green
  RGB565(0x19, 0x3D, 0x3F),  // 11. #193D3F - Teal
  RGB565(0x4F, 0x67, 0x81),  // 12. #4F6781 - Blue-gray
  RGB565(0xAF, 0xBF, 0xD2),  // 13. #AFBFD2 - Light blue
  RGB565(0xFF, 0xFF, 0xFF),  // 14. #FFFFFF - White
  RGB565(0x2C, 0xE8, 0xF4),  // 15. #2CE8F4 - Cyan
  RGB565(0x04, 0x84, 0xD1)   // 16. #0484D1 - Blue
};

// Palette 3: DAWNBRINGER-16 (popular 16-color palette)
const uint16_t PALETTE_DAWNBRINGER16[16] PROGMEM = {
  RGB565(0x14, 0x0C, 0x1C),  // 1. #140C1C - Very dark purple
  RGB565(0x44, 0x24, 0x34),  // 2. #442434 - Dark purple
  RGB565(0x30, 0x34, 0x6D),  // 3. #30346D - Dark blue
  RGB565(0x4E, 0x4A, 0x4E),  // 4. #4E4A4E - Dark gray
  RGB565(0x85, 0x4C, 0x30),  // 5. #854C30 - Brown
  RGB565(0x34, 0x65, 0x24),  // 6. #346524 - Dark green
  RGB565(0xD0, 0x46, 0x48),  // 7. #D04648 - Red
  RGB565(0x75, 0x71, 0x61),  // 8. #757161 - Gray
  RGB565(0x59, 0x7D, 0xCE),  // 9. #597DCE - Blue
  RGB565(0xD2, 0x7D, 0x2C),  // 10. #D27D2C - Orange
  RGB565(0x85, 0x95, 0xA1),  // 11. #8595A1 - Blue-gray
  RGB565(0x6D, 0xAA, 0x2C),  // 12. #6DAA2C - Green
  RGB565(0xD2, 0xAA, 0x99),  // 13. #D2AA99 - Tan
  RGB565(0x6D, 0xC2, 0xCA),  // 14. #6DC2CA - Cyan
  RGB565(0xDA, 0xD4, 0x5E),  // 15. #DAD45E - Yellow
  RGB565(0xDE, 0xEE, 0xD6)   // 16. #DEEED6 - Off-white
};

// Palette 4: WOODSPARK (16-color nature palette)
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

// Palette 5: LOST CENTURY (16-color vintage palette)
const uint16_t PALETTE_LOSTCENTURY[16] PROGMEM = {
  RGB565(0xD1, 0xB1, 0x87),  // 1. #D1B187 - Tan
  RGB565(0xC7, 0x7B, 0x58),  // 2. #C77B58 - Light brown
  RGB565(0xAE, 0x5D, 0x40),  // 3. #AE5D40 - Brown
  RGB565(0x79, 0x44, 0x4A),  // 4. #79444A - Dark red-brown
  RGB565(0x4B, 0x3D, 0x44),  // 5. #4B3D44 - Very dark brown
  RGB565(0xBA, 0x91, 0x58),  // 6. #BA9158 - Gold-brown
  RGB565(0x92, 0x74, 0x41),  // 7. #927441 - Dark gold
  RGB565(0x4D, 0x45, 0x39),  // 8. #4D4539 - Dark gray-brown
  RGB565(0x77, 0x74, 0x3B),  // 9. #77743B - Olive
  RGB565(0xB3, 0xA5, 0x55),  // 10. #B3A555 - Yellow-green
  RGB565(0xD2, 0xC9, 0xA5),  // 11. #D2C9A5 - Light tan
  RGB565(0x8C, 0xAB, 0xA1),  // 12. #8CABA1 - Blue-gray
  RGB565(0x4B, 0x72, 0x6E),  // 13. #4B726E - Dark teal
  RGB565(0x57, 0x48, 0x52),  // 14. #574852 - Dark purple-gray
  RGB565(0x84, 0x78, 0x75),  // 15. #847875 - Gray
  RGB565(0xAB, 0x9B, 0x8E)   // 16. #AB9B8E - Light gray
};

// Palette 6: BERRY NEBULA (8-color purple gradient)
const uint16_t PALETTE_BERRYNEBULA[16] PROGMEM = {
  RGB565(0x6C, 0xED, 0xED),  // 1. #6CEDED - Cyan
  RGB565(0x6C, 0xB9, 0xC9),  // 2. #6CB9C9 - Light blue
  RGB565(0x6D, 0x85, 0xA5),  // 3. #6D85A5 - Blue
  RGB565(0x6E, 0x51, 0x81),  // 4. #6E5181 - Purple-blue
  RGB565(0x6F, 0x1D, 0x5C),  // 5. #6F1D5C - Purple
  RGB565(0x4F, 0x14, 0x46),  // 6. #4F1446 - Dark purple
  RGB565(0x2E, 0x0A, 0x30),  // 7. #2E0A30 - Very dark purple
  RGB565(0x0D, 0x00, 0x1A),  // 8. #0D001A - Black-purple
  // Indices 9-16 repeat the 8 colors
  RGB565(0x6C, 0xED, 0xED),  // 9. Maps to color 1
  RGB565(0x6C, 0xB9, 0xC9),  // 10. Maps to color 2
  RGB565(0x6D, 0x85, 0xA5),  // 11. Maps to color 3
  RGB565(0x6E, 0x51, 0x81),  // 12. Maps to color 4
  RGB565(0x6F, 0x1D, 0x5C),  // 13. Maps to color 5
  RGB565(0x4F, 0x14, 0x46),  // 14. Maps to color 6
  RGB565(0x2E, 0x0A, 0x30),  // 15. Maps to color 7
  RGB565(0x0D, 0x00, 0x1A)   // 16. Maps to color 8
};

// Palette 7: GOTHIC BIT (8-color gray/purple tones)
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

// Palette 8: DREAM HAZE (8-color purple/pink gradient)
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

// Palette 9: LINK'S AWAKENING (4-color Zelda inspired)
const uint16_t PALETTE_LINKSAWAKENING[16] PROGMEM = {
  RGB565(0x5A, 0x39, 0x21),  // 1. #5A3921 - Dark brown
  RGB565(0x6B, 0x8C, 0x42),  // 2. #6B8C42 - Olive green
  RGB565(0x7B, 0xC6, 0x7B),  // 3. #7BC67B - Light green
  RGB565(0xFF, 0xFF, 0xB5),  // 4. #FFFFB5 - Cream yellow
  // Repeat for compatibility with 16-color system
  RGB565(0x5A, 0x39, 0x21),  // 5. Maps to color 1
  RGB565(0x6B, 0x8C, 0x42),  // 6. Maps to color 2
  RGB565(0x7B, 0xC6, 0x7B),  // 7. Maps to color 3
  RGB565(0xFF, 0xFF, 0xB5),  // 8. Maps to color 4
  RGB565(0x5A, 0x39, 0x21),  // 9. Maps to color 1
  RGB565(0x6B, 0x8C, 0x42),  // 10. Maps to color 2
  RGB565(0x7B, 0xC6, 0x7B),  // 11. Maps to color 3
  RGB565(0xFF, 0xFF, 0xB5),  // 12. Maps to color 4
  RGB565(0x5A, 0x39, 0x21),  // 13. Maps to color 1
  RGB565(0x6B, 0x8C, 0x42),  // 14. Maps to color 2
  RGB565(0x7B, 0xC6, 0x7B),  // 15. Maps to color 3
  RGB565(0xFF, 0xFF, 0xB5)   // 16. Maps to color 4
};

// Palette 10: ICE CREAM GB (4-color pastel palette)
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

// Palette 11: HOLLOW (4-color minimal palette)
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

// Palette 12: NINTENDO GAMEBOY (4-color classic Game Boy palette)
const uint16_t PALETTE_GAMEBOY[16] PROGMEM = {
  RGB565(0x08, 0x18, 0x20),  // 1. #081820 - Very dark teal
  RGB565(0x34, 0x68, 0x56),  // 2. #346856 - Dark green
  RGB565(0x88, 0xC0, 0x70),  // 3. #88C070 - Light green
  RGB565(0xE0, 0xF8, 0xD0),  // 4. #E0F8D0 - Very light green
  // Repeat for compatibility with 16-color system
  RGB565(0x08, 0x18, 0x20),  // 5. Maps to color 1
  RGB565(0x34, 0x68, 0x56),  // 6. Maps to color 2
  RGB565(0x88, 0xC0, 0x70),  // 7. Maps to color 3
  RGB565(0xE0, 0xF8, 0xD0),  // 8. Maps to color 4
  RGB565(0x08, 0x18, 0x20),  // 9. Maps to color 1
  RGB565(0x34, 0x68, 0x56),  // 10. Maps to color 2
  RGB565(0x88, 0xC0, 0x70),  // 11. Maps to color 3
  RGB565(0xE0, 0xF8, 0xD0),  // 12. Maps to color 4
  RGB565(0x08, 0x18, 0x20),  // 13. Maps to color 1
  RGB565(0x34, 0x68, 0x56),  // 14. Maps to color 2
  RGB565(0x88, 0xC0, 0x70),  // 15. Maps to color 3
  RGB565(0xE0, 0xF8, 0xD0)   // 16. Maps to color 4
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
const int NUM_PALETTES = 13;
const uint16_t* PALETTE_CATALOG[NUM_PALETTES] = {
  // 16-color palettes (6 total)
  PALETTE_SWEETIE16,
  PALETTE_PICO8,
  PALETTE_ENDESGA16,
  PALETTE_DAWNBRINGER16,
  PALETTE_WOODSPARK,
  PALETTE_LOSTCENTURY,
  // 8-color palettes (3 total)
  PALETTE_BERRYNEBULA,
  PALETTE_GOTHICBIT,
  PALETTE_DREAMHAZE,
  // 4-color palettes (4 total)
  PALETTE_LINKSAWAKENING,
  PALETTE_ICECREAMGB,
  PALETTE_HOLLOW,
  PALETTE_GAMEBOY
};

const char* PALETTE_NAMES[NUM_PALETTES] = {
  // 16-color palettes
  "SWEETIE-16",
  "PICO-8",
  "ENDESGA-16",
  "DAWNBRINGER",
  "WOODSPARK",
  "LOST CENTURY",
  // 8-color palettes
  "BERRY NEBULA",
  "GOTHIC BIT",
  "DREAM HAZE",
  // 4-color palettes
  "LINK'S AWK",
  "ICE CREAM",
  "HOLLOW",
  "GAME BOY"
};

// Palette sizes (4, 8, or 16 colors)
// Organized by size: 16-color, then 8-color, then 4-color
const uint8_t PALETTE_SIZES[NUM_PALETTES] = {
  // 16-color palettes
  16,  // SWEETIE-16
  16,  // PICO-8
  16,  // ENDESGA-16
  16,  // DAWNBRINGER-16
  16,  // WOODSPARK
  16,  // LOST CENTURY
  // 8-color palettes
  8,   // BERRY NEBULA
  8,   // GOTHIC BIT
  8,   // DREAM HAZE
  // 4-color palettes
  4,   // LINK'S AWAKENING
  4,   // ICE CREAM
  4,   // HOLLOW
  4    // GAME BOY
};

#endif // PALETTES_H
