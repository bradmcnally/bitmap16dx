#pragma once

#include <cstdint>

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 240
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 135
#endif

#if !defined(ARDUINO)
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t*>(address))
#endif
#ifndef pgm_read_word
#define pgm_read_word(address) (*reinterpret_cast<const uint16_t*>(address))
#endif
#endif
