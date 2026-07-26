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
battery reporting, IMU, and the Grove RGB matrix signal adapter.

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

The desktop and Zero simulators use these controls. The Linux device target
uses the same non-matrix commands; physical matrix output awaits the Grove
signal adapter.

- `H`, `T`, `V`, `P`, and `O`: Help, Settings, Preview, Palette, and Memory
- `Alt+B`: Charging
- `Esc`: return to Canvas; `Q`: quit
- Arrow keys: move the cursor or navigate the current view
- `Enter` or `Space`: draw/activate; `Backspace` or `Delete`: erase
- `1`-`8`: colors 1-8; `Alt`+`1`-`8`: colors 9-16
- `S`: save; `Alt`+`S`: save as a new sketch; `N`: new sketch
- `X`: export at 128×128; `Alt+X`: export at the logical grid size
- `C`: next color; `F`: fill; `Z`: undo; `G`: grid size; `R`: rulers
- `+` / `-`: integer canvas zoom in/out; moving at a viewport edge pans
- Hold `M` with an arrow to move the artwork
- `K` or `Alt+Backspace`: clear the canvas
- Hold `B` with `+` or `-`: display brightness
- Hold `L` with `Enter`: matrix on/off; hold `L` with `+` or `-`: matrix
  brightness
- `Y`: simulate shake-to-undo when Shake Undo is enabled

In Memory, `Delete` removes the selected sketch, `Z` restores the most recent
deletion, and `V` opens the saved-sketch slideshow. Slideshow arrows move
between sketches and `Space` toggles three-second auto-advance.

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

1. Draw with arrows plus Enter, erase, fill, undo, switch grid size, toggle
   rulers, move with `M`+arrows, and clear with `K`.
2. Select colors 1-8 and `Alt`+1-8, cycle colors, apply built-in palettes,
   then add a `.hex` file and verify the `U` user-palette filter.
3. Save, save-as, create a new sketch, reopen both sketches, delete one, and
   restore it with `Z`.
4. Export scaled and logical PNGs and inspect the `exports/` directory.
5. Enter Memory and start the slideshow with `V`; test arrows, backgrounds
   1-4, `Space` autoplay, and return to the same Memory selection.
6. Change every Settings row and restart to confirm persistence.
7. Toggle and configure the matrix window; check 8×8, scaled 8×8 on four
   units, 16×16 on four units, all rotations, and brightness.
8. Test Help return navigation, Preview, Charging, display brightness, both
   themes, `Y` shake simulation, Escape behavior, and `Q` quit.
