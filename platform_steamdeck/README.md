# Steam Deck quick test target

This target runs the existing SDL application fullscreen with a 320x200
logical framebuffer. Steam Deck's 1280x800 panel scales it by an exact 4x.
It is intended for sideloaded controller testing before Steamworks packaging.

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

## Provisional controller layout

| Control | Canvas | Menus |
|---|---|---|
| Left stick | Move canvas cursor | Navigate |
| Right stick | Move color cursor by visible row/column | Browse Palettes left/right |
| A | Draw; hold while moving to paint | Activate/apply |
| X | Erase; hold while moving to erase | Delete selected sketch |
| Y | Flood fill | — |
| B | Back | Back |
| LT + left stick | Move artwork | — |
| R3 | Open Palettes | — |
| L3 | Toggle 8x8/16x16 grid | — |
| L1 | Save | — |
| R1 | Preview | — |
| View | Open Sketches | Close Sketches |
| Menu | Open Settings | Close Settings |
| D-pad | Move canvas cursor | Navigate |

Right-stick color movement follows the palette rail geometry. Up/down changes
rows. Left/right switches columns only for a 16-color palette.

Keyboard controls remain available in this quick build.

## First Deck test

1. Confirm the window fills 1280x800 and retains sharp integer-scaled pixels.
2. Confirm SDL logs a controller and every control above responds.
3. Hold A or X while moving the left stick.
4. Hold LT while moving the left stick and verify the move cursor and wrapped
   artwork movement.
5. Move the right stick through 4-, 8-, and 16-color palette rails.
6. Press R3, browse palettes with the right stick, apply with A, and cancel
   with B.
7. Save with L1, open Sketches with View, and reopen the saved sketch.
8. Test both Gaming Mode and Desktop Mode, then report any incorrect button
   labels, dead-zone problems, double movements, or missed repeats.
