# Development

Build, packaging, feed configuration, and device validation instructions.
Run commands from the repository root unless a section says otherwise.

## Project structure

```
bitmap16dx/
├── platformio.ini          # PlatformIO configuration
├── CMakeLists.txt          # SDL development builds
├── platform_adv/          # ADV manual and platform resources
├── platform_desktop/       # Shared desktop/Steam Deck/Zero SDL shell
├── platform_steamdeck/     # Steam Deck packaging and sideload files
├── platform_zero/          # Cardputer Zero simulator/runtime files
├── src/
│   ├── main.cpp            # Cardputer ADV application
│   ├── core/               # Shared editor, views, codecs, and settings
│   ├── platform_adv/       # ADV hardware adapters
│   ├── palettes.h          # Built-in palette definitions
│   ├── icons.h             # UI icons
│   └── boot_image.h        # Splash screen
├── test/                   # Native shared-core tests
└── docs/                   # Porting and release validation documents
```

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
./build_zero/bitmap16dx_desktop
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

See [the Steam Deck sideload guide](../platform_steamdeck/README.md) for the
controller layout and copy/run instructions.

## Zero build and packaging

### Build on Cardputer Zero

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

### Package and reinstall from your Mac


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

## Steam Deck build

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
8. Confirm L4 opens Resize Canvas, L5 toggles Preview, and R4 opens Help then
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
