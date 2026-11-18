# BitMap16 DX iOS Porting Plan

## Goal

Ship a touch-first BitMap16 DX app for iPhone and iPad, then distribute a tested
build through TestFlight and submit it to the App Store. Keep sketch formats and
editing behavior compatible with the existing platforms.

## Starting point (2026-11-17)

- Drawing, editing, palettes, views, PNG export, and workspace persistence are
  already mostly portable C++.
- The current SDL2 application shell is in `platform_desktop/main_desktop.cpp`.
  It assumes a desktop window and uses keyboard, mouse, and controller actions.
- `CanvasView::cellAtPointer` and the desktop mouse path already support direct
  drawing, but the menus and tools still need touch controls.
- `Workspace::dataRoot()` currently uses XDG or HOME paths. iOS needs a path in
  its app container.
- Xcode 27.1 and the iPhoneOS and iPhoneSimulator SDKs are installed at
  `/Volumes/Super Disk System/Developer/Xcode.app`. Build commands can set
  `DEVELOPER_DIR` to its `Contents/Developer` directory without changing the
  system-wide Xcode selection.

## Technical direction

Keep the C++ editor, sketch format, renderers, and workspace. Add a dedicated
`platform_ios` target using SDL2 for the app window, rendering, and touch events.
Use a small Objective-C++ bridge only where Apple APIs are needed: app container
paths, document import/export, sharing, and lifecycle integration. Avoid adding
iOS branches throughout the editor and view logic; put touch commands and
platform services behind narrow interfaces.

Start in landscape with a logical framebuffer chosen after measuring the
smallest supported iPhone and an iPad. Preserve crisp pixel scaling and respect
safe areas. Treat portrait support as a product decision after the landscape
prototype is usable.

## Milestones

### 1. Bootable development build

1. Add an Xcode project or CMake/Xcode configuration for an iOS app bundle with
   SDL2 linked for device and simulator.
2. Compile the shared C++ sources, load the bundled assets, create the renderer,
   and show the Canvas view in the iPhone simulator.
3. Handle app background and foreground transitions without losing the current
   sketch or leaving input held.

**Done when:** the app launches from Xcode in the simulator, shows a sharp full
screen canvas, and survives suspend/resume. This is the first implementation
checkpoint; it needs no signing or store account.

### 2. Touch editing and navigation

1. Map SDL finger events to logical canvas coordinates. Track finger IDs so a
   drawing stroke remains one undo action and a second finger cannot paint
   accidentally. Avoid processing both synthetic mouse and finger events for
   the same touch.
2. Add visible, finger-sized controls for color, draw/erase, fill, undo/redo,
   grid, move, zoom, save, Sketches, Palettes, Preview, and Settings.
3. Define gestures explicitly: one finger paints or erases with the selected
   tool; a separate pan/zoom mode handles canvas movement. Add pinch zoom only
   after it can be tested without conflicting with strokes.
4. Adapt Canvas, Sketches, Palettes, Settings, Help, and Preview layouts for
   touch targets and safe areas. Replace keyboard help with iOS instructions.
5. Keep external keyboard and controller input where it works, but make every
   core action available without them.

**Done when:** a user can create, edit, undo, save, reopen, and export an 8x8,
16x16, and 32x32 sketch using touch alone on both phone and tablet simulators.

### 3. iOS files and sharing

1. Put sketches, imported palettes, settings, and exports in appropriate
   persistent app-container locations. Keep the existing `.dat`, `.hex`, and
   PNG formats; do not silently change existing files.
2. Add Files import for `.hex` palettes and existing sketches, with validation
   before copying into the workspace.
3. Add a Share/Save to Files action for exported PNGs. Decide whether raw
   sketch files should also be user-exportable for migration and backup.
4. Confirm that reinstall, app updates, and low-storage failures do not silently
   destroy sketches. Treat delete/restore and failed writes explicitly.

**Done when:** sketches and settings persist across relaunches, a palette can
be imported from Files, and an exported PNG opens outside the app.

### 4. Device quality pass

1. Test on at least one physical iPhone and one iPad, including different
   screen ratios, safe areas, touch latency, rotation, backgrounding, memory
   pressure, and interrupted exports.
2. Run existing portable tests and desktop/Zero/Deck build checks after shared
   changes. Add focused tests for touch-to-command mapping and workspace path
   selection where they protect cross-platform behavior.
3. Review accessibility: control labels, contrast, touch target sizes, and
   alternatives to gestures. Verify that hardware-specific settings such as
   RGB matrix and charging mode are absent or clearly adapted on iOS.
4. Prepare app icon, launch presentation, support/privacy links, screenshots,
   and App Store metadata. Record which data the app actually collects.

**Done when:** the complete drawing workflow passes on physical devices and no
existing platform regressions are known.

### 5. Beta and release

1. Set the app identifier, version, signing, and distribution configuration.
2. Archive and upload a build to App Store Connect; distribute through
   TestFlight for feedback.
3. Fix beta findings, complete the App Store privacy and age-rating details,
   and submit the release build for review.

**Done when:** the app is approved and available in the intended App Store
regions. Developer Program enrollment and account access are required at this
stage, not for the first simulator build.

## Key risks and decisions

- **Touch design is the largest product change.** The desktop mouse canvas is
  a useful starting point, but the keyboard-based menu flow is not a usable
  phone interface by itself.
- **Small displays need a deliberate layout.** Scaling the Cardputer or Deck
  framebuffer unchanged may leave controls too small to tap.
- **File access changes on iOS.** Existing HOME/XDG paths and SD-card-style
  import assumptions must be replaced with app-container storage and Files
  integration.
- **Physical-device validation is required before release.** Simulator success
  will not establish touch feel, frame pacing, or safe-area behavior.

## Recommended first work package

Build Milestone 1 as a thin vertical slice: add the iOS target and SDL2
dependency, boot into the existing Canvas renderer, set an app-container
workspace path, and document one simulator build command. This will expose
compile and packaging issues early while leaving the current platforms intact.

## References

- SDL2 iOS integration: https://wiki.libsdl.org/SDL2/README-ios
- SDL2 touch events: https://wiki.libsdl.org/SDL2/README-touch
- Apple app-container directories: https://developer.apple.com/documentation/foundation/url/documentsdirectory
- TestFlight: https://developer.apple.com/testflight/
- App Store submission: https://developer.apple.com/app-store/submitting/
