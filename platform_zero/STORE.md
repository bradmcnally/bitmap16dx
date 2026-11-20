# Cardputer Zero store release

The listing is defined in [`app-builder.json`](../app-builder.json). It uses
**BitMap16 DX**, package name **bitmap16dx**, category **Creative & Office**,
and share code **16DX**. Keep the package name and share code stable after the
first publication. Availability is checked by the store at submission time.

## Build and check

From the repository root:

```sh
python3 platform_zero/tools/check_store.py
./platform_zero/device.sh package pi@cardputerzero.local
```

The package command builds on the Zero and checks the resulting
`build_zero_device/bitmap16dx_0.8.0-1_arm64.deb`. Local installation uses the
same package. To check a package again:

```sh
python3 platform_zero/tools/check_store.py --deb build_zero_device/bitmap16dx_0.8.0-1_arm64.deb
```

The checks cover listing assets, version, package name, architecture, the ARM64
executable, launcher files, install locations, and license files. They do not
replace the store's validation or physical-device testing.

To build directly on the Zero, use `cmake --preset zero-device`,
`cmake --build --preset zero-device`, then `cpack --config build_zero_device/CPackConfig.cmake`.
The AppBuilder online build currently expects SCons; use this project's native
CMake/CPack package for submission.

## Listing assets

The listing uses a 256×256 icon and three 320×170 PNG screenshots in
[`store/`](store/). They are rendered from the application's embedded pencil
and UI views, using a sample drawing. The app launcher uses a separate
128×128 PNG in `launcher/bitmap16dx.png`. Regenerate the assets after UI changes:

```sh
cmake --preset zero
cmake --build --preset zero --target bitmap16dx_store_assets
./build_zero_simulator/bitmap16dx_store_assets platform_zero/store platform_zero/launcher/bitmap16dx.png
python3 platform_zero/tools/check_store.py
```

The Zero declares IMU access for shake undo and network access for Lospec.
The other five permissions are false. It launches as the regular device user;
there is no background service. Installation uses `sudo`, but launching the
application does not.

Settings are stored in `${XDG_CONFIG_HOME:-$HOME/.config}/bitmap16dx/settings.bin`.
Sketches and palettes are stored in `${XDG_DATA_HOME:-$HOME/.local/share}/bitmap16dx/`.
New PNG exports are saved in `$HOME/Pictures/BitMap16DX/`.
`BITMAP16_DATA_DIR` keeps a test workspace self-contained; use
`BITMAP16_CONFIG_DIR` to override its configuration folder separately.

## Test on your Zero

Install and launch from HOME as your regular user. Check drawing, erase/fill,
undo/redo, save and reopen, palette import and browsing, PNG export, and quit.
Check tap-Escape navigation and hold-Escape returning HOME after three seconds.
Release early to check that the countdown cancels; repeat from text entry and
confirmation prompts. Check that settings and existing artwork survive an
update. Verify optional shake undo on a Zero with an IMU.

Record a short **physical-device video** showing launch from HOME, drawing,
palettes, saving or reopening a sketch, and exiting back to HOME. The store
requires this video for the first submission; simulator screenshots do not
replace it.

## Submit

Choose one route after testing:

- Open [Developer Center](https://dev.cardputer.cc/), sign in, upload the
  versioned `.deb`, and enter the listing values from `app-builder.json`.
- With the official [AppBuilder CLI](https://github.com/CardputerZero/AppBuilder)
  installed and signed in, run its `czdev publish --deb` command **from this
  repository root**, pointing to the versioned `.deb`.

Both routes create a submission pull request in
[CardputerZero/packages](https://github.com/CardputerZero/packages/pulls).
Add the physical-device video to that pull request's comments. A store
maintainer reviews the first release.

Before later releases, update the version in `app-builder.json`; CPack reads
its version and Debian revision from that file. Confirm the package passes
the checks before submitting it.

Sources: [development guidelines](https://cardputer.cc/#/documents/cp0-dev),
[submission guide](https://cardputer.cc/#/documents/cp0-submit), and
[metadata schema](https://github.com/CardputerZero/AppBuilder/blob/main/docs/APP_BUILDER_JSON.md).
