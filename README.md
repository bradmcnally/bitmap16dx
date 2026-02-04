![Logo](img/bitmap16dx.png)

BitMap16 DX is a tiny pixel art tool for the M5Stack Cardputer ADV. 

![Drawing Mode](img/drawing.png)

## Features
- 8×8 and 16×16 canvas modes
- Draw, Erase, Fill tools
- Undo last action
- Save/open sketches from SD card
- Built-in 16, 8, and 4-color palettes
- Export `.png` files to `bitmap16dx/exports/` (128x128 or logical size)

### First Boot

1. **Insert SD card** before powering on (FAT32 format recommended)
2. Power on your Cardputer
3. BitMap16 DX will automatically create these folders:
   ```
   /bitmap16dx/
   ├── sketches/   # Your saved artwork
   ├── exports/    # Exported PNG files
   └── palettes/   # Custom color palettes (optional)
   ```
4. Start drawing!

### Adding Custom Palettes (Optional)
![Available Palettes](img/palettes.png)
1. Download `.hex` palette files from [Lospec](https://lospec.com/palette-list)
2. Copy to `/bitmap16dx/palettes/` on your SD card
3. Power cycle your Cardputer
4. Access custom palettes in the Palette Menu

*Supports 4, 8, or 16-color palettes. Maximum 32 total (12 built-in + 20 custom).*

## How to Use

### Drawing Mode

![Drawing Interface](img/drawing_link.png)

| Key | Function |
|-----|----------|
| Arrow keys (`↑` `←` `↓` `→`) | Move cursor (hold to repeat) |
| `ok`/`enter` | Place pixel with selected color |
| `del`/`backspace` | Erase pixel |
| `1-8` | Quick color selection (colors 1-8) |
| `fn + 1-8` | Quick color selection (colors 9-16) |
| `C` | **C**ycle to next color |
| `F` | Flood **f**ill |
| `G` | Toggle between 8×8 and 16×16 **g**rid |
| `Z` | Undo last action |
| `g0` button | Clear canvas |
| `S` | **S**ave sketch (update current or create new) |
| `FN + S` | **S**ave as new sketch (always creates new file) |
| `X` | E**x**port PNG (128×128 scaled) |
| `FN + X` | Export PNG (logical size: 8×8 or 16×16) |
| `I` | Open help screen (key commands) |
| `P` | Open **P**alette Menu |
| `O` | **O**pen Sketches Menu |
| `V` | Open Pre**v**iew Mode |

### Palette Menu *(P)*

| Key | Function |
|-----|----------|
| `←`/`→` | Navigate palettes |
| `ok`/`enter` | Select palette |
| `esc` | Dismiss |
| `4` | Toggle 4-color filter |
| `8` | Toggle 8-color filter |
| `1` | Toggle 16-color filter|
| `U` | Toggle custom user palettes filter (can be combined with 4/8/16 filters) |
| `0` | Clear all filters |

### Sketches Menu *(O)*

![Sketches Menu](img/sketches.png)

| Key | Function |
|-----|----------|
| Arrow keys (`↑` `←` `↓` `→`) | Navigate sketch grid |
| `ok`/`enter` | Load selected sketch |
| `esc` | Dismiss |
| `g0` button | Delete focused sketch |
| `z`  | undo |

### Preview Mode *(V)*

| Key | Function |
|-----|----------|
| `1` | Black background |
| `2` | White background |
| `3` | Gray background |
| `esc` | Dismiss |

### Project Structure

```
BitMap16DX/
├── platformio.ini          # PlatformIO configuration
├── src/
│   ├── main.cpp           # Main firmware code
│   ├── palettes.h         # Default Color palette definitions
│   ├── icons.h            # UI icons
│   ├── cartridge_graphic.h # Cartridge sprite
│   └── boot_image.h       # Splash screen
```

### What's Next
- I2C module support (joystick/buttons)
- Bluetooth keyboard/controller support
- Background music/UI sound exploration