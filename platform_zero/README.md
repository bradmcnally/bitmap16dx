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
reporting, BMI270 shake undo, and the Grove RGB matrix signal adapter.

## Package and reinstall from your Mac


After installing the build dependencies on the Zero, run these commands from
the BitMap16 DX source folder on your Mac. Replace `pi@cardputerzero.local`
with your Zero's username and IP address if needed.

```sh
./platform_zero/device.sh package pi@cardputerzero.local
./platform_zero/device.sh install pi@cardputerzero.local
```

The first command builds on the Zero and saves the resulting package locally
as `build_zero_device/bitmap16dx_arm64.deb`. Run it again when the source
changes. The second command copies and reinstalls the cached package, including
rebuilds with the same version. Quit BitMap16 DX before installing and reopen
it from HOME afterward.

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

**Shake undo** in Settings uses the standard Zero's BMI270 accelerometer.
Shake the device in Canvas to undo the last edit. It uses the ADV's threshold
above 6 g and 500 ms cooldown. Sampling continues while no keys are pressed.
The setting is hidden automatically on the Zero Lite and whenever a usable
accelerometer is not detected at startup. The saved preference is retained.

The app discovers the BMI270 by its Linux IIO name (`bmi27` or `bmi270`),
reads acceleration through `/sys/bus/iio/devices/iio:device*/`, and converts
the scaled readings from m/s² to g. It needs read access to `in_accel_scale`
and `in_accel_{x,y,z}_raw`. A range of ±8 g or ±16 g is required. If the
image starts with a smaller range, the app selects an advertised ±8 g range
(or ±16 g) when the scale file is writable. Otherwise it hides the setting
and records the reason in `/tmp/bitmap16dx.log`. Configure the sensor range
or permissions through the device image rather than running the app as root.
The BMM150 compass sensor is not needed for shake undo. The simulator retains
the `Y` shortcut for a simulated shake; on the physical Zero, `Y` no longer
simulates motion.

In Settings, **Warmth** cycles through **Neutral**, **Mild**, **Warm**, and
**Warmer**. Use Left/Right or Enter to change it; the display updates immediately
and remembers the choice. Start with Mild for a slightly cool-looking LCD.
Neutral restores the original colors. This affects only Bitmap16DX's LCD output,
including its UI; palettes, saved sketches, and exported PNGs keep their original
colors. Framebuffer screenshots include the display tint. The
desktop simulator and other applications are unaffected.

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
| `Ctrl+B` | Open Screensaver. Press a regular key to return to Canvas. |
| `Fn+U` / `Fn+I` | Dim / brighten the LCD when the app has access to the backlight control. |
| Escape | Go back. In Help, return to the previous view. In a slideshow, return to Sketches. At startup, skip the splash screen. |
| `Q` | Quit. At the unsaved-changes prompt, use Left/Right to choose, Enter to confirm, or Escape to cancel. |

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
| `G` | Open Resize Canvas. Use Left and Right to select 8×8, 16×16, or 32×32, then Enter to focus Apply and Enter again to confirm. Up returns to the size row; Escape cancels. |
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
| Sketches | Backspace or `Fn+Backspace` | Open confirmation to move the selected saved sketch to trash. |
| Sketches | `U` / `Ctrl+S` | Restore the most recently deleted sketch / duplicate the selected sketch. |
| Palette | Left and Right / Enter | Browse and apply palettes. |
| Palette | `4`, `8`, `1`, `U`, `0` | Filter to 4, 5–8, or 9–16 colors or user palettes, or clear all filters. |
| Preview and slideshow | `Fn+L` / `Fn+M` | Zoom in / out. |
| Preview and slideshow | `1`, `2`, `3`, `4` | Choose a black, white, gray, or dark-gray background. |
| Slideshow | Left and Right / Space | Browse saved sketches / toggle three-second auto-advance. |

The resize picker outlines the part of the canvas that will stay visible
when shrinking. It warns if the new grid hides drawn pixels. Resize preserves
those pixels and can be undone with `Ctrl+Z`. **Grid default** in Settings
sets the starting size for new sketches.
The `>` marker identifies the currently applied size. When focus moves to
Cancel or Apply, an outline keeps the pending size visible.

Dialogs use arrow-key focus and Enter to select. Save warnings start on Save;
sketch-deletion confirmation starts on Cancel. Focused buttons have white text
on black in light theme and black text on white in dark theme.

In Settings, set **Battery display** to **Off** to hide the Canvas battery
icon. While the Zero reports charging, the charging symbol replaces the
battery icon in the same status slot. The battery returns when charging stops.
The setting defaults to **On** and persists between launches.
Screensaver includes the floppy and skull icons and shows the charging symbol
only while charging. Battery status continues updating while it runs.

In the Zero simulator, use desktop arrows for the `Fn` arrow combinations,
`F1`–`F8` for colors 9–16, Delete for `Fn+Backspace`, and Page Up and Page
Down for `Fn+L` and `Fn+M`. Use `B` with `+` or `-` for simulated display
brightness. RGB matrix support, its settings, and its simulator window are
disabled in Zero builds.

See the [ADV and Zero key comparison](CONTROLS.md) for platform differences.

Memory starts with a `+` tile for a new sketch, followed by saved sketches.
The newest saved sketch opens automatically on the next launch.

Copy Lospec `.hex` files containing 4–16 colors into `palettes/`.
Opening the Palette view reloads them. The `U` filter shows user palettes.
The Zero supports up to 116 saved user palettes, alongside the 12 built-in
palettes.

In the local Palette view, select a user cartridge and press Backspace or
Delete to remove it. Built-in palettes are protected. Removed palette files
move to `trash/`; saved artwork and the current drawing retain their own
palette colors. A deleted Lospec palette can appear in the online feed again.

Deleting a sketch from Sketches opens a confirmation with **Cancel**
selected. Use Left/Right (Zero `Z`/`C`, or controller D-pad) to choose, then
Enter/`A` to confirm. Escape/`B` cancels. This confirmation always appears,
regardless of the save-warning setting.

### Import a palette by name or URL

In either palette list, press `I` to open **Import from Lospec**. Type a
palette slug such as `endesga-16`, or paste a full Lospec palette URL with
`Ctrl+V` when a clipboard is available. This dialog uses the Zero's physical
keyboard, with no on-screen keyboard. A gray example is chosen when the
dialog opens and stays fixed; it is not submitted as input. The insertion
cursor blinks while the text field is focused. Enter moves focus to Import;
Enter again fetches the palette. Up returns to editing, and Escape cancels.
The field accepts up to 2048 characters.

A successful lookup opens an **Import Preview** cartridge. Enter saves and
applies it; Escape returns to local palettes without saving. Creator credit
is shown when Lospec supplies it. Import supports 4–16 colors
and reports missing palettes, unsupported sizes, and connection failures.
Lookup runs in the background, uses the public Lospec download endpoint
without an API key, and does not replace the daily feed cache. Saved imports
work offline like other user palettes.

### New palettes from Lospec

In Palette, press `L` for Lospec, then `R` to refresh. Left/Right browse
all downloaded palettes in one continuous carousel; Enter saves the selected
palette and applies it with the insertion animation. Press `L` to return to
local palettes. Escape also returns to local Palettes; a second Escape closes
Palettes and returns to Canvas. The `4`, `8`, and `1` filters show 4, 5–8,
and 9–16 colors respectively;
`0` clears the filters. These filters also work in Lospec.
Saved palettes are omitted from the new-palette feed. There is no likes filter.

Browsing and filtering use the local cache without network requests. Refresh
runs in the background, at most once a minute, and a failed refresh keeps the
previous cache. Saved palettes remain available offline.

For releases, use the shared [Cloudflare palette feed](../services/lospec-feed/README.md).
Your key stays in a Cloudflare secret; app users do not need a Lospec account.
Build with the public feed URL:

```sh
cmake --preset zero -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
```

For device packaging from your Mac, set `BITMAP16_LOSPEC_FEED_URL` when
running `platform_zero/device.sh package`. The script forwards that URL to
the device build. The hosted feed is one request per refresh.

For direct API development, configure with `-DBITMAP16_LOSPEC_FEED_URL=`
to disable the default hosted feed, then put your key alone on one line
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

The Zero simulator opens only the main app window; RGB matrix support is disabled.

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
7. Test palette import, 4 / 5–8 / 9–16 filters, saved-palette deletion, and
   Escape navigation from Lospec to local Palettes to Canvas.
8. Test Help return navigation, Preview, `Ctrl+B` Screensaver, display
   brightness, both themes, Escape behavior, and `Q` quit.
