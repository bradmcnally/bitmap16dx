# Steam Deck guide

Steam / Steam Deck is coming soon. This guide covers the development build.

## Install

Build the Linux executable using the [Steam Deck build instructions](../docs/development.md#steam-deck-build).
Copy it to `~/Applications/bitmap16dx/` on the Deck using removable storage
or SSH. From the Linux build machine:

```sh
ssh deck@steamdeck.local 'mkdir -p ~/Applications/bitmap16dx'
scp build_steamdeck/bitmap16dx_desktop \
  deck@steamdeck.local:Applications/bitmap16dx/
ssh deck@steamdeck.local \
  'chmod +x ~/Applications/bitmap16dx/bitmap16dx_desktop'
```

Replace the hostname or username if your Deck uses different ones.

## First launch

In Desktop Mode, add `~/Applications/bitmap16dx/bitmap16dx_desktop` to Steam
as a non-Steam game and select Steam Input's standard Gamepad template.
Launch through Steam in Gaming Mode or Desktop Mode.

For an existing Distrobox setup, run **Update and Build** or run
`platform_steamdeck/update_and_build.sh` inside the `dev` container. It pulls
the latest source, builds, runs checks, and installs to
`~/Applications/bitmap16dx/bitmap16dx_desktop`, the existing Steam shortcut target.
Failed builds or checks leave the installed app available. `build-info.txt`
beside the executable identifies the installed source revision.

Sketches and palettes are stored in `~/.local/share/bitmap16dx/`. Settings use
`~/.config/bitmap16dx/`, preserving existing Deck preferences on first launch.
Exports and screenshots go to `~/Pictures/BitMap16DX/`; existing files in the
old exports folder stay there. Use Quit in Settings to close the app.

## Controls

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
| L3 | Play / pause music | Play / pause music, including dialogs |
| L1 / LB | Undo on release | Previous palette color-range filter |
| R1 / RB | Redo | Next palette color-range filter |
| L2 + R2 | Save once per chord | Zoom out / in individually |
| View | Open Sketches | Close Sketches |
| Menu | Open Settings | Close Settings |
| D-pad | Move canvas cursor with hold repeat | Navigate; in Palettes, Up toggles user palettes and Down clears filters |
| L4 | Open Resize Canvas | Import from Lospec in Palettes |
| L5 | Toggle Preview | Toggle Preview |
| R4 | Toggle Help | Toggle Help |
| R5 | Toggle grid rulers | — |

Right-stick color movement follows the palette rail geometry. Up/down changes
rows. Left/right switches columns for palettes with more than eight colors.

L4 resizes the canvas. Resizing is undoable. L3 pauses or resumes music from
the current playback position throughout the app, including modal dialogs.

Keyboard controls are also available.

Settings > Audio includes Sound effects on/off and Music on/off, followed by
Music volume and Sound effects volume together. Left/right adjusts each level
from 0 to 100% in 10% steps, independently of system volume. Levels are saved;
defaults are 100% effects and 30% music.
Tracks are labeled BGM01, BGM02, and so on.
The bundled MP3 playlist repeats in filename order throughout the app;
music starts off and its preference is saved. Keyboard media keys also control
play/pause and previous/next. Sound effects play over the music.
When copying a build manually, copy the build's `music/` folder alongside
`bitmap16dx_desktop`. Update and Build installs both automatically.

Help groups controls into Draw, Canvas, Palettes, Sketches, Preview, App, and
Keyboard sections. Use LB/RB to change sections and Up/Down to select a control.
In Palettes, Import stays at the left of the footer and Browse Lospec at the right.

In local Palettes, press `X` (keyboard Backspace/Delete) to remove a saved
user palette. Removed files move to `trash/`.
Existing artwork retains its palette colors, including when reopened later.

## Using the app

See [Using Bitmap16 DX](../README.md#using-bitmap16-dx) for the shared workflow.

### Import a palette by name or URL

In Palettes, press `L4` (keyboard `I`) to import a palette slug such as
`endesga-16` or a full Lospec palette URL. Review the returned palette, then
press Enter / `A` to save and apply it, or Escape / `B` to return to the palette
list. Palettes with 4–16 colors are supported; saved imports remain available offline.

### Lospec palettes

In Palettes (`R3`), press `Y` to switch between local and Lospec cartridges.
Press `A` to save and apply a palette. Left/right stick or D-pad browses
all downloaded cartridges in one continuous carousel. Saved palettes join
the local collection and are omitted from the feed.

The feed refreshes locally on the first visit each day. Cached results
remain available offline, including if a refresh fails.
Keyboard filters `4`, `8`, and `1` show 4, 5–8, and 9–16 colors respectively;
`0` clears the filters. Controller LB/RB cycles All → 4 → 5–8 → 9–16
(forward or backward, wrapping at the ends). D-pad Up toggles saved-user
palettes in the local list; D-pad Down clears the filters in either list.
Each cartridge shows its actual color count. `B` returns
from Lospec to local Palettes; press `B` again to return to Canvas.

### Trackpad and additional controls

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

### Export a screenshot

Press `F12` to write a UI-free 1280x800 PNG to:

```text
~/Pictures/BitMap16DX/
```

The artwork is centered at the largest integer scale that fits the Steam Deck
screen. Transparent pixels use the active theme background, or the selected
Preview background when exporting from Preview. The saved image excludes the
cursor, tools, palette rail, rulers, labels, and all other UI.

## Troubleshooting

- If the executable will not run, use an x86-64 Linux build and check that
  it has executable permission. A macOS build cannot run on the Deck.
- If controller or trackpad input is missing, launch through Steam and check
  the game's Steam Input layout.
- If rear grips do not respond, bind L4, L5, R4, and R5 to their corresponding
  gamepad paddle inputs in Steam Input.
- If Lospec cannot refresh, cached palettes remain available offline.

For build checks and controller validation, see the
[development guide](../docs/development.md#steam-deck-validation).
