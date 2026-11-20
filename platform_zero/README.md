# Cardputer Zero guide

## Install

When the Zero release is available, get BitMap16 DX from either the
[GitHub Releases page](https://github.com/bradmcnally/bitmap16dx/releases)
or its [Cardputer Zero Store listing](https://cardputer.cc/#/apps/16DX)
(coming soon).

If you download the ARM64 Debian package from GitHub, install it on the Zero:

```sh
sudo dpkg -i bitmap16dx_0.9.0-1_arm64.deb
```

To create the package from source or update over SSH, follow the
[Zero build and packaging instructions](../docs/development.md#zero-build-and-packaging).
Quit BitMap16 DX before reinstalling.

## First launch

Open BitMap16 DX from HOME. The app creates its data folders automatically.
Press `Q` to quit. Sketches and palettes are stored in:

```text
~/.local/share/bitmap16dx/
├── palettes/
├── trash/
└── sketches/
```

Settings are stored separately in `~/.config/bitmap16dx/settings.bin`.
New PNG exports are saved in `~/Pictures/BitMap16DX/`, under the current user's
home folder. Exports made with older versions remain in
`~/.local/share/bitmap16dx/exports/`.

## Controls

<a id="cardputer-zero-keyboard-reference"></a>

Use `Ctrl` for BitMap16 DX commands such as save, undo, and export. Use
`Fn` for shortcuts that use the Zero's arrow, function, Delete, Page Up,
Page Down, or backlight keys.

| Direction | Keys |
|---|---|
| Up | `F` or `Fn+F` |
| Left | `Z` or `Fn+Z` |
| Down | `X` or `Fn+X` |
| Right | `C` or `Fn+C` |

### General

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
| Escape | Tap to go back. Hold for 3 seconds to return HOME; release early to cancel the countdown. At startup, tap to skip the splash screen. |
| `Q` | Quit. |

### Canvas

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
| `Ctrl+X` / `Ctrl+L` | Export a 128×128 PNG / Native PNG at the original canvas size. |

### Sketches, Palette, and Preview

| View | Key | Action |
|---|---|---|
| Sketches | Direction keys / Enter | Select and open a sketch or create one using the `+` tile. |
| Sketches | Backspace or `Fn+Backspace` | Move the selected saved sketch to trash. |
| Sketches | `U` / `Ctrl+S` | Restore the most recently deleted sketch / duplicate the selected sketch. |
| Palette | Left and Right / Enter | Browse and apply palettes. |
| Palette | `4`, `8`, `1`, `U`, `0` | Filter to 4, 5–8, or 9–16 colors or user palettes, or clear all filters. |
| Preview and slideshow | `Fn+L` / `Fn+M` | Zoom in / out. |
| Preview and slideshow | `1`, `2`, `3`, `4` | Choose a black, white, gray, or dark-gray background. |
| Slideshow | Left and Right / Space | Browse saved sketches / toggle three-second auto-advance. |

See the [ADV and Zero key comparison](CONTROLS.md) for platform differences.

## Using the app

See [Using Bitmap16 DX](../README.md#using-bitmap16-dx) for the shared workflow.

Sketches starts with a `+` tile for a new sketch, followed by saved sketches.
Each launch starts with a new sketch; saved artwork remains in Sketches.

Copy Lospec `.hex` files containing 4–16 colors into `palettes/`.
Opening the Palette view reloads them. The `U` filter shows user palettes.
The Zero supports up to 116 saved user palettes, alongside the 12 built-in
palettes.

In the local Palette view, select a user cartridge and press Backspace or
Delete to remove it. Removed palette files move to `trash/`; saved artwork and the current drawing retain their own
palette colors. A deleted Lospec palette can appear in the online feed again.

### Device settings

Enable **Shake undo** in Settings to undo an edit by shaking the Zero.
This option is available on devices with a supported accelerometer.

In Settings, set **Battery display** to **Off** to hide the Canvas battery
icon. While the Zero reports charging, the charging symbol replaces the
battery icon in the same status slot. The battery returns when charging stops.
The setting defaults to **On** and persists between launches.

### Import a palette by name or URL

In either palette list, press `I` to import a palette slug such as
`endesga-16` or a full Lospec palette URL. A successful import saves and
applies the palette immediately. Palettes with 4–16 colors are supported,
and saved imports remain available offline.

### New palettes from Lospec

In Palette, press `L` for Lospec. Left/Right browses the feed; Enter saves
and applies the selected palette. Escape returns to local Palettes; a
second Escape returns to Canvas. The `4`, `8`, and `1` filters show 4, 5–8,
and 9–16 colors respectively; `0` clears the filters.
Saved palettes are omitted from the feed.

The feed refreshes locally on the first visit each day. If a refresh fails,
the previous cache remains available.

## Troubleshooting

- If the app does not open, inspect `/tmp/bitmap16dx.log`.
- If the LCD or keypad is unavailable, check device permissions using the
  [Zero build instructions](../docs/development.md#zero-build-and-packaging).
- Shake undo is available when a supported accelerometer is detected. If the
  setting is missing, see [Zero sensor diagnostics](../docs/development.md#zero-sensor-diagnostics).
- If Lospec cannot refresh, keep using the cached feed or saved palettes and
  try again when connected.

For simulator setup and checks, see the [development guide](../docs/development.md#complete-simulator-validation).
