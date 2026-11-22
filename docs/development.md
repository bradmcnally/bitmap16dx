# Development

Build, packaging, feed configuration, and device validation instructions.
Run commands from the repository root unless a section says otherwise.

## Project structure

```text
bitmap16dx/
├── README.md               # Overview and platform guides
├── CMakeLists.txt          # Desktop, Zero, and Steam Deck builds
├── CMakePresets.json       # Build presets
├── platformio.ini          # ADV firmware build
├── app-builder.json        # Zero store metadata and package version
├── assets/audio/           # Shared sound effects and music for SDL platforms
├── src/
│   ├── main.cpp            # ADV entry point
│   ├── core/               # Shared editor and UI
│   └── platform_adv/       # ADV hardware adapters
├── platform_adv/           # ADV manual and hardware references
├── platform_desktop/       # Shared SDL runtime
│   └── tests/              # Desktop and persistence tests
├── platform_zero/          # Zero device input, display, and sensors
│   ├── launcher/           # Launcher entry, icon, and startup script
│   ├── store/              # Store icon and screenshots
│   ├── tools/              # Store checks and asset generation
│   ├── tests/              # Zero input and sensor tests
│   └── device.sh           # Build and install over SSH
├── platform_steamdeck/     # Steam Deck guide and controller layout
├── test/test_core/         # PlatformIO shared-core tests
├── palettes/               # Bundled palette source files
├── services/lospec-feed/   # Cloudflare palette feed
├── third_party/            # External library sources and licenses
└── docs/
    ├── development.md      # Build and validation instructions
    ├── images/             # Manual screenshots, photos, and video
    └── plans/              # Future platform plans
```

Generated output goes into `build/desktop`, `build_zero_simulator`,
`build_zero_device`, and `build_steamdeck`. PlatformIO uses `.pio/`.
Older local outputs are kept under `build/archive`, `build/releases`, or
`build_zero_device/archive`; they are not part of the source repository.

The shared `Editor` owns the live sketch, cursor, selected color, and undo/redo
history on ADV and the SDL platforms. ADV keeps view state, file identity,
hardware effects, and storage outside the editor; saving and rendering read the
editor's sketch directly. ESP32 flood fill uses static scratch buffers because
edits run serially and the firmware loop has limited stack space.

The SDL `PaletteBrowser` owns palette catalog snapshots, Lospec requests and
cache freshness, refresh cooldown, filters, and save-before-apply decisions.
Its download adapter supplies asynchronous results; the same interface accepts
controlled offline results in tests. SDL retains input mapping, frame pacing,
rendering, and navigation between views. Catalog backing data remains valid
across workspace palette reloads.

Desktop, Zero, and Steam Deck share audio assets in `assets/audio/`; MP3 music
files belong in `assets/audio/music/`. The [audio source inventory](../assets/audio/README.md)
explains licenses and restoring the private Cyberleaf effects for a fresh checkout.
Public sources, credits and checksums are tracked separately from those ignored
WAVs. Audio credits are copied into each build's `music/` folder and included by
the Zero package and Steam Deck installer. SDL builds preload
`assets/audio/transition_soft_02b.wav` for the start of a cartridge slide and
`assets/audio/success_melodic_01b.wav` for the first completed insertion frame.
The completion cue plays once, before the existing brief hold and return to
Canvas. CMake embeds both WAVs into the executable, including Zero packages, so no runtime asset path or
extra effect files are needed. SDL2_mixer 2.6+ streams MP3 music from the
`music/` directory beside the executable, while effects mix over it. CMake
copies `assets/audio/music/*.mp3` into that directory in filename order.
The Zero package and Steam Deck installer include the directory. Music is off
by default; Settings > Audio has Sound effects and Music on/off toggles, followed
by Music volume and Sound effects volume together. These percentages adjust
independent mixer gains in 10%
steps, clamped to 0–100%, without changing system volume or playback state.
Effects default to 100%, and music to 30%; navigation retains its quieter sample
gain within the effects mix. Master volume and previous/next track use key
commands. Tracks display as BGM01, BGM02, and so on.
Audio preferences are stored in version 7 SDL settings,
with defaults preserved for older files. Playback and media controls are
serviced while waiting for events, including idle screens and modal dialogs.
Sound effects default to on and can be disabled independently of music.
Unavailable audio leaves the app usable and logs the reason.

Install SDL2_mixer development files before building: `brew install sdl2_mixer`
on macOS, or `sudo apt install libsdl2-mixer-dev` on Linux. The Zero package
also declares the SDL2_mixer runtime dependency.

Opening Palettes, Sketches, Settings, or Help plays `enable_02.wav`; closing
plays `disable_02.wav`. Successfully opening an existing sketch from Sketches
plays `enable_04a.wav` instead of the dismiss sound. Canceled or failed opens
stay in Sketches. Applying a cartridge keeps its dedicated success cue.
These menu sounds are embedded and use a separate effect channel.
Successful Save, Save As, and Save from the unsaved-changes prompt play the
embedded `success_melodic_03b.wav` confirmation. Failed or canceled saves
stay silent. This cue follows the effects toggle and mix, and plays separately
from menu cues so Save and Open can complete without cutting it off.

Navigation uses an embedded array of `keys_minimal_01.wav` through
`keys_minimal_05.wav`. Actual menu-focus and color-swatch changes choose a
random sample, avoid immediate repeats, and share a 100 ms cooldown.
Ticks play at half effect volume on their own channel, leaving cartridge cues
and background music uninterrupted. Drawing-cursor movement stays silent.

On the physical Zero, Fn+S/D and USB volume keys adjust PipeWire's default
output with `wpctl`, matching beepbotdx and the HOME volume setting. The
native display adapter overlays the percentage without closing or redrawing
modal dialogs. Volume changes in five-percent steps, capped at 100%; missing
PipeWire output reports Volume unavailable. The `zero_system_volume` test
uses a controlled `wpctl` helper to check shared-state reads and failures.

The shared `SketchWorkflow` owns unsaved-change tracking and pending New, Open,
and Quit decisions. ADV and SDL storage adapters perform file operations, while
their input and rendering code presents the save warning. Open retains a file
identity across save-driven catalog reordering; failed saves and failed opens
preserve the current drawing. SDL compares against the saved sketch; ADV retains
its existing edit-since-save warning policy. Losing a saved copy counts as unsaved
on both platforms. Restoring a deleted SDL file only reattaches it if its detached
sketch is still current.

For planned iPhone and iPad support, see the [iOS plan](plans/ios.md).


## Build and install the ADV firmware

The PlatformIO environment is named `m5stack-cardputer` and builds the
Cardputer ADV firmware:

```sh
pio run -e m5stack-cardputer
```

To upload to an attached ADV:

```sh
pio run -e m5stack-cardputer -t upload
```

PlatformIO detects the USB serial port automatically. If detection fails or
multiple devices are connected, run `pio device list` and add
`--upload-port <device>` to the upload command, replacing `<device>` with the
listed device path.


## Cardputer Zero device build

See the [Cardputer Zero build and install guide](../platform_zero/README.md)
to build the Linux package and launch it from HOME.

## Desktop and Cardputer Zero simulator

The repository includes SDL2 simulators for the 240×135 Cardputer ADV layout
and the 320×170 Cardputer Zero layout:

```sh
cmake --preset desktop
cmake --build --preset desktop
./build/desktop/bitmap16dx_desktop

cmake --preset zero
cmake --build --preset zero
./build_zero_simulator/bitmap16dx_desktop
```

The simulator persists sketches, exports, settings, and custom palettes under
`~/.local/share/bitmap16dx`. The desktop ADV simulator also opens an RGB-matrix
simulation window. RGB matrix support and its controls are disabled for Zero
and Steam Deck builds.

Icons are embedded directly in the application; no separate icon files are needed at runtime.

## Steam Deck development build

The Deck preset renders a 320×200 logical framebuffer at an exact 4× scale on
the 1280×800 display and enables SDL controller input:

```sh
cmake --preset steamdeck
cmake --build --preset steamdeck
```

On a Deck with the existing `dev` Distrobox container, the **Update and Build**
launcher runs `platform_steamdeck/update_and_build.sh`. The script pulls with
`--ff-only`, builds, runs CTest, verifies a Linux x86-64 executable, and updates
`~/Applications/bitmap16dx/bitmap16dx_desktop`. A failed build or check preserves
the installed executable. Set `BITMAP16_STEAM_DECK_INSTALL_DIR` for a different
installation directory. The script resets stale feed settings to the preset
default; set the environment variable `BITMAP16_LOSPEC_FEED_URL` to keep an
explicit override, including an empty value for direct API development.

The installer keeps Steam's shortcut path as a launcher and installs the
Linux binary as `bitmap16dx_desktop.bin` beside it. The launcher enters the
`dev` Distrobox without a terminal so runtime dependencies match the build.
Running an Arch-built binary directly on SteamOS can fail before startup if
libraries such as SDL2_mixer are only installed in the container. Set
`BITMAP16_STEAM_DECK_CONTAINER` to use another prepared container.

See [the Steam Deck sideload guide](../platform_steamdeck/README.md) for the
controller layout and copy/run instructions.

## Zero build and packaging

For store metadata, assets, validation, and submission, see the
[Zero store release guide](../platform_zero/STORE.md).

### Build on Cardputer Zero

Install the native build dependencies:

```sh
sudo apt update
sudo apt install cmake build-essential libsdl2-dev libsdl2-mixer-dev libcurl4-openssl-dev ca-certificates
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

Open BitMap16 DX from HOME. Hold Escape for three seconds to return HOME and release the keypad to
the system. If the app does not open, inspect `/tmp/bitmap16dx.log`. The
launch user needs write access to `/dev/fb_lcd` (or `/dev/fb0`) and read
access to the keypad's input event device. The brightness shortcut also
requires write access to `/sys/class/backlight/backlight/brightness`.

USB keyboards also require read access to their `/dev/input/event*` devices.
The Zero adapter discovers keyboards by their evdev key capabilities every
half second, tracks held keys per device, and translates standard USB keys
separately from the built-in keypad's Sym codes. No USB library is needed.
The `zero_usb_keyboard` test checks merged holds, repeat timing, modifiers,
Delete and numpad Enter mappings, keyboard-specific navigation, and
disconnect/reconnect behavior. The `zero_keyboard_help` test verifies that
the active keyboard's labels fit the Help screen. Built-in events retain
their source so only their F/Z/X/C keys become arrows; USB letters stay letters
in every view and dialog. Help follows the last keyboard used and refreshes
on USB connection/disconnection, including while idle.

### Package and reinstall from your Mac


After installing the build dependencies on the Zero, run these commands from
the BitMap16 DX source folder on your Mac. Replace `pi@cardputerzero.local`
with your Zero's username and IP address if needed.

```sh
./platform_zero/device.sh package pi@cardputerzero.local
./platform_zero/device.sh install pi@cardputerzero.local
```

The first command builds on the Zero and saves the resulting package locally
as `build_zero_device/bitmap16dx_<version>_arm64.deb`. Run it again when the source
changes. The second command copies and reinstalls the cached package, including
rebuilds with the same version. Quit BitMap16 DX before installing and reopen
it from HOME afterward.


## Zero feed configuration and simulator checks

For releases, use the [Cloudflare palette feed](../services/lospec-feed/README.md).
Build with the public feed URL:

```sh
cmake --preset zero -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
```

For device packaging from your Mac, set `BITMAP16_LOSPEC_FEED_URL` when
running `platform_zero/device.sh package`. The script forwards that URL to
the device build.

For direct API development, configure with `-DBITMAP16_LOSPEC_FEED_URL=`
to disable the default hosted feed, then put your key alone on one line
in `~/.local/share/bitmap16dx/lospec-key.txt`. The build can bundle that file;
override its location with `-DBITMAP16_LOSPEC_KEY_FILE=/absolute/path/to/key.txt`.
Do not commit the key. Direct mode checks up to three batches of 100, stopping
at 20 unsaved matches. `BITMAP16_LOSPEC_API_KEY` or `lospec-key.txt` in the
active data folder can override the bundled key for testing. A hosted-feed
build does not bundle or send a Lospec key.

## Complete simulator validation

For device and release checks, use the [QA checklist](QA_CHECKLIST.md).

Build and launch the 320×170 Zero simulator:

```sh
cmake --preset zero
cmake --build --preset zero
./build_zero_simulator/bitmap16dx_desktop
```

The Zero simulator opens only the main app window; RGB matrix support is disabled.

For an isolated test that does not touch the normal workspace:

```sh
BITMAP16_DATA_DIR=/tmp/bitmap16dx-zero-test \
  ./build_zero_simulator/bitmap16dx_desktop
```

Recommended all-at-once test:

1. Navigate with desktop arrows. Draw with Enter or Space, erase
   with Backspace, fill with `Ctrl+F`, erase fill with `E`, undo/redo with
   `Ctrl+Z`/`Ctrl+Y`, jump with `Shift+Arrow`, move with `M+Arrow`, and
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
   brightness, both themes, Escape behavior, and holding Escape to return HOME.

## Steam Deck build

Build on an x86-64 Linux machine with SDL2 development files installed:

```sh
sudo apt update
sudo apt install cmake build-essential libsdl2-dev libsdl2-mixer-dev libcurl4-openssl-dev ca-certificates

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


## Steam Deck feed configuration

For the Steam release, deploy the [Cloudflare feed](../services/lospec-feed/README.md)
and configure its public URL before building:

```sh
cmake --preset steamdeck \
  -DBITMAP16_LOSPEC_FEED_URL=https://YOUR-WORKER.workers.dev/palettes.json
cmake --build --preset steamdeck
```

The public feed is configured by default. To use direct API development,
set `-DBITMAP16_LOSPEC_FEED_URL=` and use the
same optional key setup as the [Zero feed configuration](#zero-feed-configuration-and-simulator-checks).


## Steam Deck validation

1. Confirm the window fills 1280x800 and retains sharp integer-scaled pixels.
2. Confirm SDL logs a controller and every control above responds.
3. Hold A or X while moving the left stick.
4. Hold LT while moving the left stick and verify the move cursor and wrapped
   artwork movement.
5. Move the right stick through palette rails with 4–16 colors, including
   a partially filled second column such as a 14-color palette.
6. Press R3, browse palettes with the right stick, apply with A, and cancel
   with B. Also verify left stick, D-pad, and keyboard arrows browse the
   carousel, and that A applies the selected palette before
   returning to the canvas.
7. Draw, release L1 to undo, and press R1 to redo. Confirm L2+R2 saves without
   also undoing, then open Sketches with View and reopen the saved sketch.
8. Confirm L3 pauses/resumes music in every view and dialog, L4 opens Resize Canvas,
   L5 toggles Preview, and R4 opens Help then
   returns to the previous screen. Confirm R5 toggles the grid rulers.
9. Test both Gaming Mode and Desktop Mode, then report any incorrect button
   labels, dead-zone problems, double movements, or missed repeats.
10. Press F12 and inspect the resulting 1280x800 PNG in `~/Pictures/BitMap16DX/`. Confirm it
    contains only integer-scaled artwork and the selected background.
11. In Help, use LB/RB to visit every section and confirm controller labels and
    footer prompts stay clear of the content. In local Palettes, Import belongs
    at the left and Browse Lospec at the right, matching Zero's alignment.
12. Open Import with L4 and leave the system keyboard up through several cursor
    blinks. Confirm the dialog stays visible, type a slug, then check Cancel
    and Import focus transitions without flashing or reopening the keyboard.

SDL exposes the Deck's rear grips as controller paddles. If Steam Input does
not pass them through with the selected template, bind L4, L5, R4, and R5 to
their corresponding gamepad paddle inputs in the game's controller layout.

### Text-input smoke check

Run the Steam Deck text-input smoke check against native SDL2, as on the Deck.
The macOS SDL2-compat package cannot inject synthetic `SDL_TEXTINPUT` events;
the smoke check reports that limitation instead of exercising that unsupported
library path. Normal system keyboard events do not use synthetic injection.

## Zero sensor diagnostics

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


## Zero simulator controls

In the Zero simulator, use desktop arrows for the `Fn` arrow combinations,
`F1`–`F8` for colors 9–16, Delete for `Fn+Backspace`, and Page Up and Page
Down for `Fn+L` and `Fn+M`. Use `B` with `+` or `-` for simulated display
brightness. RGB matrix support, its settings, and its simulator window are
disabled in Zero builds.
The simulator uses the USB navigation and Help layout: F/Z/X/C remain letters.
