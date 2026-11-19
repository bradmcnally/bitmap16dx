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
sudo apt install cmake build-essential libsdl2-dev libcurl4-openssl-dev ca-certificates

cmake --preset steamdeck
cmake --build --preset steamdeck
```

The Linux executable is:

```text
build_steamdeck/bitmap16dx_desktop
```

For a 1280x800 windowed preview on a development computer, run:

```sh
build_steamdeck/bitmap16dx_desktop --windowed
```

A macOS build made with this preset is useful for compilation and controller
testing on macOS, but it cannot run on Steam Deck. Build the executable on
x86-64 Linux for sideloading.
Include `third_party/picojson/picojson.h` with distributed builds to retain
the bundled JSON reader's license notice.

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
| L3 | Open Resize Canvas | Import from Lospec in Palettes |
| L1 / LB | Undo on release | Previous palette color-range filter |
| R1 / RB | Redo | Next palette color-range filter |
| L2 + R2 | Save once per chord | Zoom out / in individually |
| View | Open Sketches | Close Sketches |
| Menu | Open Settings | Close Settings |
| D-pad | Move canvas cursor with hold repeat | Navigate; in Palettes, Up toggles user palettes and Down clears filters |
| L4 | Open Resize Canvas | — |
| L5 | Toggle Preview | Toggle Preview |
| R4 | Toggle Help | Toggle Help |
| R5 | Toggle grid rulers | — |

Right-stick color movement follows the palette rail geometry. Up/down changes
rows. Left/right switches columns for palettes with more than eight colors.

Dialogs use D-pad Left/Right to move focus, `A` to select, and `B` to cancel.
Save warnings start on Save; sketch-deletion confirmation starts on Cancel.

L3 or L4 opens Resize Canvas. D-pad Left/Right chooses 8×8, 16×16, or 32×32.
`A` moves focus to Apply; `A` again applies the change. D-pad Up returns to
the size row, Down returns to the buttons, and `B` cancels without changing
the artwork. The `>` marker stays on the applied size; an outline marks the
pending size while the action buttons have focus. Resizing is undoable.

Keyboard controls remain available in this quick build.

In local Palettes, press `X` (keyboard Backspace/Delete) to remove a saved
user palette. Built-in palettes are protected. Removed files move to `trash/`.
Existing artwork retains its palette colors, including when reopened later.

Deleting a sketch from Sketches opens a confirmation with **Cancel**
selected. Use Left/Right (Zero `Z`/`C`, or controller D-pad) to choose, then
Enter/`A` to confirm. Escape/`B` cancels. This confirmation always appears,
regardless of the save-warning setting.

## Import a palette by name or URL

In Palettes, press `L3` (keyboard `I`) to open **Import from Lospec**.
The app uses the SteamOS keyboard rather than drawing its own. Open the
[system keyboard with Steam + X](https://help.steampowered.com/en/faqs/view/671A-4453-E8D2-323C)
while the field is focused, or type with a physical keyboard. `Ctrl+V` pastes
when a clipboard is available. Type a short name such as `endesga-16` or a
full Lospec palette URL.

The dialog contains the text field and Cancel/Import buttons. Enter / `A`
moves from the field to Import; select it again to fetch. Up returns to the
field, Left/Right chooses between buttons, and Escape / `B` cancels. The
empty field shows a fixed gray example, and its cursor blinks while editing.

The result appears as a single **Import Preview** cartridge, with creator
credit when available. Press `A` to save and apply it or `B` to cancel.
Only palettes with 4–16 colors can be imported. No player API key or
Cloudflare change is required, and imported palettes remain available offline.
The daily palette feed cache is preserved.

## Lospec palettes

In Palettes (`R3`), press `Y` to switch between local and Lospec cartridges.
Press `X` to refresh and `A` to save and apply a palette. Left/right stick
or D-pad browses all downloaded cartridges in one continuous carousel.
Keyboard equivalents are `P`, `L`, `R`, and Enter. Palettes with 4–16 colors appear; there is no likes filter. Saved palettes are omitted from
Lospec and join the local cartridge collection (up to 116 user palettes).
Browsing and color filters use cached results without network requests.
Results persist offline. Refresh is manual and runs in the background.
Keyboard filters `4`, `8`, and `1` show 4, 5–8, and 9–16 colors respectively;
`0` clears the filters. Controller LB/RB cycles All → 4 → 5–8 → 9–16
(forward or backward, wrapping at the ends). D-pad Up toggles saved-user
palettes in the local list; D-pad Down clears the filters in either list.
Each cartridge shows its actual color count. `B` returns
from Lospec to local Palettes; press `B` again to return to Canvas.
The focused cartridge rises 8 pixels. Switching lists and filters uses a
center-out reload; opening Palettes uses the full entrance wave.

For the Steam release, deploy the [shared Cloudflare feed](../services/lospec-feed/README.md)
and configure its public URL before building:

```sh
cmake --preset steamdeck \
  -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
cmake --build --preset steamdeck
```

This build contains the public feed URL and no Lospec key. Players do not
need a Lospec account. The public feed is configured by default. To use direct API development,
set `-DBITMAP16_LOSPEC_FEED_URL=` and use the
same optional key setup as the [Zero build](../platform_zero/README.md#new-palettes-from-lospec).

## Trackpad and additional controls

For pointer control, set the **left trackpad** to **Mouse** in Steam Input and
bind its click to **Left Mouse Click**, retaining the other gamepad bindings.
In Gaming Mode, select Bitmap16 DX in the library, open the controller icon,
then **Edit Layout → Trackpads**. Set **Left Trackpad Behavior** to **As Mouse**
and **Left Trackpad Click** to **Left Mouse Click**. Launch through Steam so
the per-game layout is active.
Moving the trackpad positions the canvas cursor. Hold trackpad click or `A`
to draw, or hold `X` to erase while moving. `Y` fills once per press at the
current cursor position. A continuous draw or erase hold is one undo step.
Desktop mouse movement uses the same path: left-click/drag draws and
right-click/drag erases. Pointer movement outside the artwork does not paint.
Canvas prompts follow the last keyboard or controller input. Keyboard prompts
show `RET` (Enter), `BS` (Backspace), `F` (fill), and `P` (Palettes).
Stick movement must exceed the input dead zone before switching prompts.
Canvas zoom is disabled on Steam Deck, including keyboard `+` / `-`.
Preview zoom remains available through LT / RT and keyboard `+` / `-`.
The Steam Deck Settings menu includes a `Quit` row for closing the app
without using a keyboard or the Steam overlay. It also includes a `Help` row
that opens the controller reference and returns to Settings when closed.

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
5. Move the right stick through palette rails with 4–16 colors, including
   a partially filled second column such as a 14-color palette.
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

### Text-input smoke check

Run the Steam Deck text-input smoke check against native SDL2, as on the Deck.
The macOS SDL2-compat package cannot inject synthetic `SDL_TEXTINPUT` events;
the smoke check reports that limitation instead of exercising that unsupported
library path. Normal system keyboard events do not use synthetic injection.
