# Steam Deck quick test target

This target runs the existing SDL application fullscreen with a 320x200
logical framebuffer. Steam Deck's 1280x800 panel scales it by an exact 4x.
It is intended for sideloaded controller testing before Steamworks packaging.
Other display resolutions use strict integer scaling. Any letterbox or
pillarbox margins are filled with the active view's background color.

## Build

Build on an x86-64 Linux machine with SDL2 development files installed:

```sh
sudo apt update
sudo apt install cmake build-essential libsdl2-dev

cmake --preset steamdeck
cmake --build --preset steamdeck
```

The Linux executable is:

```text
build_steamdeck/bitmap16dx_desktop
```

A macOS build made with this preset is useful for compilation and controller
testing on macOS, but it cannot run on Steam Deck. Build the executable on
x86-64 Linux for sideloading.

## Sideload

Enable SSH on the Deck or copy the executable with removable storage. An SSH
example from the Linux build machine:

```sh
ssh deck@steamdeck.local 'mkdir -p ~/Applications/bitmap16dx'
scp build_steamdeck/bitmap16dx_desktop \
  deck@steamdeck.local:Applications/bitmap16dx/
ssh deck@steamdeck.local \
  'chmod +x ~/Applications/bitmap16dx/bitmap16dx_desktop'
```

Run it once from a Desktop Mode terminal:

```sh
~/Applications/bitmap16dx/bitmap16dx_desktop
```

To test in Gaming Mode, add that executable as a non-Steam game and select
Steam Input's standard Gamepad template. The application logs the controller
name when SDL recognizes it.

Data is stored under:

```text
~/.local/share/bitmap16dx/
```

Set `BITMAP16_DATA_DIR` before launching to use an isolated test workspace.

The Deck build does not read system battery information or draw Bitmap16's
battery/charging UI; SteamOS provides the device battery indicator.

## Provisional controller layout

| Control | Canvas | Menus / Preview |
|---|---|---|
| Left stick | Move canvas cursor | Navigate, including Palettes |
| Right stick | Move color cursor by visible row/column | Browse Palettes left/right |
| A | Draw; hold while moving to paint | Activate/apply; gray background in Preview |
| X | Erase; hold while moving to erase | Delete sketch; black background in Preview |
| Y | Flood fill | White background in Preview |
| B | Back | Back |
| LT + left stick | Move artwork | — |
| LT / RT | — | Zoom out / in by one integer step in Preview |
| R3 | Open Palettes | — |
| L3 | Cycle 8x8/16x16/32x32 grid | — |
| L1 | Undo on release | — |
| R1 | Redo | — |
| L2 + R2 | Save once per chord | Zoom out / in individually |
| View | Open Sketches | Close Sketches |
| Menu | Open Settings | Close Settings |
| D-pad | Move canvas cursor with hold repeat | Navigate with hold repeat |
| L4 | Cycle 8x8/16x16/32x32 grid | — |
| L5 | Toggle Preview | Toggle Preview |
| R4 | Toggle Help | Toggle Help |
| R5 | Toggle grid rulers | — |

Right-stick color movement follows the palette rail geometry. Up/down changes
rows. Left/right switches columns only for a 16-color palette.

Keyboard controls remain available in this quick build.
The Steam Deck Settings menu includes a `Quit` row for closing the app
without using a keyboard or the Steam overlay.

## UI-free screenshot export

Press `F12` to write a UI-free 1280x800 PNG to:

```text
~/.local/share/bitmap16dx/exports/
```

The artwork is centered at the largest integer scale that fits the Steam Deck
screen. Transparent pixels use the active theme background, or the selected
Preview background when exporting from Preview. The saved image excludes the
cursor, tools, palette rail, rulers, labels, and all other UI.

This local export is the App-ID-independent screenshot path. A future optional
Steamworks adapter can submit the same RGB buffer to `ISteamScreenshots` when
an App ID is available.

## First Deck test

1. Confirm the window fills 1280x800 and retains sharp integer-scaled pixels.
2. Confirm SDL logs a controller and every control above responds.
3. Hold A or X while moving the left stick.
4. Hold LT while moving the left stick and verify the move cursor and wrapped
   artwork movement.
5. Move the right stick through 4-, 8-, and 16-color palette rails.
6. Press R3, browse palettes with the right stick, apply with A, and cancel
   with B. Also verify left stick, D-pad, and keyboard arrows browse the
   animated carousel, and that A plays the insertion animation before
   returning to the canvas.
7. Draw, release L1 to undo, and press R1 to redo. Confirm L1+L2 saves without
   also undoing, then open Sketches with View and reopen the saved sketch.
8. Confirm L4 toggles the grid, L5 toggles Preview, and R4 opens Help then
   returns to the previous screen. Confirm R5 toggles the grid rulers.
9. Test both Gaming Mode and Desktop Mode, then report any incorrect button
   labels, dead-zone problems, double movements, or missed repeats.
10. Press F12 and inspect the resulting 1280x800 PNG in `exports/`. Confirm it
    contains only integer-scaled artwork and the selected background.

SDL exposes the Deck's rear grips as controller paddles. If Steam Input does
not pass them through with the selected template, bind L4, L5, R4, and R5 to
their corresponding gamepad paddle inputs in the game's controller layout.
