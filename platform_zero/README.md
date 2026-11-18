# Cardputer Zero Linux target

Cardputer Zero uses a Raspberry Pi CM0 running 64-bit Linux. It is not an
ESP32/PlatformIO target. This target shares the simulator's UI, writes
RGB565 frames directly to the LCD, and reads the keypad through Linux evdev.
The Debian package installs an entry in APPLaunch.

## Build on Cardputer Zero

Install the native build dependencies:

```sh
sudo apt update
sudo apt install cmake build-essential libsdl2-dev libcurl4-openssl-dev ca-certificates
```

Configure and build:

```sh
cmake --preset zero-device
cmake --build --preset zero-device
```

Run directly as the regular device user, with access to the LCD and keypad:

```sh
./build_zero_device/bitmap16dx_desktop
```

Create an APPLaunch-compatible Debian package:

```sh
cd build_zero_device
cpack
```

Install the generated `arm64.deb` with `sudo dpkg -i`.

Open BitMap16 DX from HOME. Press `Q` to quit and release the keypad to
the system. If the app does not open, inspect `/tmp/bitmap16dx.log`. The
launch user needs write access to `/dev/fb_lcd` (or `/dev/fb0`) and read
access to the keypad's input event device. The brightness shortcut also
requires write access to `/sys/class/backlight/backlight/brightness`.
Hardware validation is still required for the keyboard mappings, battery
reporting, the BMM150 geomagnetic sensor, and the Grove RGB matrix signal
adapter.

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

### Cardputer Zero keyboard reference

Use `Ctrl` for BitMap16 DX commands such as save, undo, and export. Use
`Fn` for shortcuts that use the Zero's arrow, function, Delete, Page Up,
Page Down, or backlight keys.

| Direction | Keys |
|---|---|
| Up | `F` or `Fn+F` |
| Left | `Z` or `Fn+Z` |
| Down | `X` or `Fn+X` |
| Right | `C` or `Fn+C` |

#### General

| Key | Action |
|---|---|
| `H` or `Fn+H` | Toggle Help. Use Up and Down to scroll. |
| `T` | Toggle Settings. Use Up and Down to select a row, then Left, Right, Enter, or Space to change or activate it. |
| `V` | Toggle Preview. In Sketches, open the saved-sketch slideshow. |
| `P` | Toggle Palette. |
| `O` | Toggle Sketches. |
| `D` | Return to Canvas. |
| `Ctrl+B` | Open the charging display. Press a regular key to return to Canvas. |
| `Fn+U` / `Fn+I` | Dim / brighten the LCD when the app has access to the backlight control. |
| Escape | Go back. In Help, return to the previous view. In a slideshow, return to Sketches. At startup, skip the splash screen. |
| `Q` | Quit. At the unsaved-changes prompt, use `S` to save, `D` to discard, or Escape to cancel. |

#### Canvas

| Key | Action |
|---|---|
| Direction keys | Move the cursor. Hold a direction to repeat. |
| Enter / Backspace | Draw / erase a pixel. Hold the key while moving to draw or erase a line. |
| `Ctrl+F` / `E` | Flood fill / erase fill. |
| `Fn+Backspace` | Clear the canvas. |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo. |
| `1`–`8` / `Fn+1`–`Fn+8` | Select colors 1–8 / 9–16 when available in the current palette. |
| `I` | Select the next color. |
| `G` | Open Resize Canvas. Use Left and Right to select 8×8, 16×16, or 32×32, then Enter to apply or Escape to cancel. |
| `R` | Toggle rulers. |
| `Fn+L` / `Fn+M` | Zoom in / out. Canvas starts at minimum zoom and shows the zoom level while zoomed. |
| `Shift+F/Z/X/C` | Jump to the corresponding artwork edge while zoomed. |
| `M+F/Z/X/C` | Move the artwork in the corresponding direction. |
| `Ctrl+S` / `Shift+S` | Save / save as a new sketch. |
| `Ctrl+X` / `Ctrl+L` | Export a 128×128 PNG / export at the sketch's native grid size. |

#### Sketches, Palette, and Preview

| View | Key | Action |
|---|---|---|
| Sketches | Direction keys / Enter | Select and open a sketch or create one using the `+` tile. |
| Sketches | Backspace or `Fn+Backspace` | Delete the selected saved sketch. |
| Sketches | `U` / `Ctrl+S` | Restore the most recently deleted sketch / duplicate the selected sketch. |
| Palette | Left and Right / Enter | Browse and apply palettes. |
| Palette | `4`, `8`, `1`, `U`, `0` | Filter to 4, 8, or 16 colors or user palettes, or clear all filters. |
| Preview and slideshow | `Fn+L` / `Fn+M` | Zoom in / out. |
| Preview and slideshow | `1`, `2`, `3`, `4` | Choose a black, white, gray, or dark-gray background. |
| Slideshow | Left and Right / Space | Browse saved sketches / toggle three-second auto-advance. |

The resize picker outlines the part of the canvas that will stay visible
when shrinking. It warns if the new grid hides drawn pixels. Resize preserves
those pixels and can be undone with `Ctrl+Z`. **Grid default** in Settings
sets the starting size for new sketches.

In Settings, set **Battery display** to **Off** to hide the Canvas battery
icon. The setting defaults to **On** and persists between launches.

In the Zero simulator, use desktop arrows for the `Fn` arrow combinations,
`F1`–`F8` for colors 9–16, Delete for `Fn+Backspace`, and Page Up and Page
Down for `Fn+L` and `Fn+M`. Use `B` with `+` or `-` for simulated display
brightness. In the RGB matrix simulator, use `L+Enter` to toggle the matrix
and hold `L` with `+` or `-` to adjust its brightness.

See the [ADV and Zero key comparison](CONTROLS.md) for platform differences.

Memory starts with a `+` tile for a new sketch, followed by saved sketches.
The newest saved sketch opens automatically on the next launch.

Copy Lospec `.hex` files containing 4, 8, or 16 colors into `palettes/`.
Opening the Palette view reloads them. The `U` filter shows user palettes.
The Zero supports up to 116 saved user palettes, alongside the 12 built-in
palettes.

### New palettes from Lospec

In Palette, press `L` for Lospec, then `R` to refresh. Use `+` and `-` to
adjust the minimum likes in steps of five (default: 10). Left/Right browse
the same cartridges as local palettes; Enter saves the selected palette and
applies it with the insertion animation. Press `L` to return to local palettes.
The existing `4`, `8`, `1`, and `0` color filters also work in Lospec.
Saved palettes are omitted from the new-palette feed.

The newest 20 matching palettes are shown from the fetched results. Results
and the like threshold persist locally. Opening Lospec or changing the
threshold uses the cache without a network request. Refresh runs in the
background, at most once a minute, and a failed refresh keeps the previous
cache. Saved palettes remain available offline.

For releases, use the shared [Cloudflare palette feed](../services/lospec-feed/README.md).
Your key stays in a Cloudflare secret; app users do not need a Lospec account.
Build with the public feed URL:

```sh
cmake --preset zero -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
```

For device packaging from your Mac, set `BITMAP16_LOSPEC_FEED_URL` when
running `platform_zero/device.sh package`. The script forwards that URL to
the device build. The hosted feed is one request per refresh.

For direct API development without hosting, put your key alone on one line
in `~/.local/share/bitmap16dx/lospec-key.txt`. The build can bundle that file;
override its location with `-DBITMAP16_LOSPEC_KEY_FILE=/absolute/path/to/key.txt`.
Do not commit the key. Direct mode checks up to three batches of 100, stopping
at 20 unsaved matches. `BITMAP16_LOSPEC_API_KEY` or `lospec-key.txt` in the
active data folder can override the bundled key for testing. A hosted-feed
build does not bundle or send a Lospec key.

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
