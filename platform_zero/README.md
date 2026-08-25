# Cardputer Zero Linux target

Cardputer Zero uses a Raspberry Pi CM0 running 64-bit Linux. It is not an
ESP32/PlatformIO target. This target reuses the SDL2 shell at the device's
native 320x170 landscape resolution and packages it for APPLaunch.

## Build on Cardputer Zero

Install the native build dependencies:

```sh
sudo apt update
sudo apt install cmake build-essential libsdl2-dev
```

Configure and build:

```sh
cmake --preset zero-device
cmake --build --preset zero-device
```

Run directly:

```sh
SDL_VIDEODRIVER=wayland ./build_zero_device/bitmap16dx_desktop
```

Create an APPLaunch-compatible Debian package:

```sh
cd build_zero_device
cpack
```

Install the generated `arm64.deb` with `sudo dpkg -i`.

The launcher probes Wayland first, then KMSDRM, and finally SDL's offscreen
backend. Hardware validation is still required for the keyboard mappings,
battery reporting, the BMM150 geomagnetic sensor, and the Grove RGB matrix
signal adapter.

## Workspace and controls

Settings and sketches persist between launches in:

```text
~/.local/share/bitmap16dx/
├── exports/
├── palettes/
├── trash/
├── settings.bin
└── sketches/
    └── sketch-0001.dat
```

Set `BITMAP16_DATA_DIR` to use a different writable location. If
`XDG_DATA_HOME` is set, the default is `$XDG_DATA_HOME/bitmap16dx`.

The `zero` and `zero-device` presets enable the Cardputer Zero input profile.
The complete physical-key mapping is documented in
[`CONTROLS.md`](CONTROLS.md).

- `F`, `Z`, `X`, and `C`: Up, Left, Down, and Right in every navigable view
- `Fn+F/Z/X/C`: firmware arrow alternatives; use desktop arrow keys to
  simulate them
- `Shift+F/Z/X/C`: jump to an artwork edge while zoomed
- Enter: draw or activate; Backspace: erase one pixel
- `Fn+Backspace` (Delete): clear Canvas or delete the selected saved sketch
- `1`-`8`: colors 1-8; `Fn+1`-`Fn+8` (`F1`-`F8`): colors 9-16
- `Ctrl+F`: fill; `E`: erase fill; `Ctrl+Z`: undo; `Ctrl+Y`: redo
- `I`: next color; `G`: grid size; `R`: rulers; `+` / `-`: zoom
- Hold `M` with `F/Z/X/C` to move the artwork
- `Ctrl+S`: save; `Shift+S`: save as a new sketch
- `Ctrl+X`: export at 128×128; `Ctrl+L`: export at logical grid size
- `H`, `T`, `V`, `P`, and `O`: Help, Settings, Preview, Palette, and Sketches
- `Ctrl+B`: charging display
- Hold `B` with `+` or `-`: display brightness
- Hold `L` with Enter: matrix on/off; hold `L` with `+` or `-`: matrix
  brightness
- Escape: return to Canvas; `Q`: quit

In Sketches, Delete removes the selected sketch, `U` restores the most recent
deletion, `Ctrl+S` duplicates the selected sketch, and `V` opens the saved-
sketch slideshow. Slideshow Left/Right moves between sketches and Space
toggles three-second auto-advance.

Memory starts with a `+` tile for a new sketch, followed by saved sketches.
The newest saved sketch opens automatically on the next launch.

Copy Lospec `.hex` files containing 4, 8, or 16 colors into `palettes/`.
Opening the Palette view reloads them. The `U` filter shows user palettes.

## Complete simulator validation

Build and launch the 320×170 Zero simulator:

```sh
cmake --preset zero
cmake --build --preset zero
./build_zero/bitmap16dx_desktop
```

The second window is the Grove RGB-matrix simulator. It uses the same
single/quad-unit mapping and rotation code as the firmware.

For an isolated test that does not touch the normal workspace:

```sh
BITMAP16_DATA_DIR=/tmp/bitmap16dx-zero-test \
  ./build_zero/bitmap16dx_desktop
```

Recommended all-at-once test:

1. Navigate with both `F/Z/X/C` and desktop arrows. Draw with Enter, erase
   with Backspace, fill with `Ctrl+F`, erase fill with `E`, undo/redo with
   `Ctrl+Z`/`Ctrl+Y`, jump with `Shift+F/Z/X/C`, move with `M+F/Z/X/C`, and
   clear with Delete.
2. Select colors 1-8 and colors 9-16 with `F1`-`F8`, cycle with `I`, apply
   built-in palettes,
   then add a `.hex` file and verify the `U` user-palette filter.
3. Save with `Ctrl+S`, save-as with `Shift+S`, create a sketch through the `+`
   slot, reopen both sketches, delete one with Delete, restore it with `U`,
   and duplicate one with `Ctrl+S`.
4. Export scaled and logical PNGs with `Ctrl+X` and `Ctrl+L`, then inspect the
   `exports/` directory.
5. Enter Memory and start the slideshow with `V`; test arrows, backgrounds
   1-4, `Space` autoplay, and return to the same Memory selection.
6. Change every Settings row and restart to confirm persistence.
7. Toggle and configure the matrix window; check 8×8, scaled 8×8 on four
   units, 16×16 on four units, all rotations, and brightness.
8. Test Help return navigation, Preview, `Ctrl+B` Charging, display
   brightness, both themes, Escape behavior, and `Q` quit.
