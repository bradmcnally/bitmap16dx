# BitMap16 DX QA checklist

Use a fresh install or isolated data folder for the first-run checks, then repeat the persistence checks with existing sketches. Record the build, device, date, and any failed step.

**Build:** __________  **Device:** __________  **Date:** __________

## First launch and navigation

- [ ] App opens to a new blank canvas after the splash screen (about 1.5 seconds); Escape skips the splash.
- [ ] Canvas, Sketches, Palettes, Preview, Settings, and Help open and return to the expected view.
- [ ] Light and dark themes render legibly; settings survive a restart.
- [ ] Unsaved-changes prompts offer the correct save, discard, and cancel behavior.

## Drawing

- [ ] Draw and erase single pixels and continuous lines; fill and erase fill stay within the selected region.
- [ ] Move artwork, clear the canvas, undo, and redo; undo history resets when opening or creating a sketch.
- [ ] Resize among 8×8, 16×16, and 32×32; shrinking and growing preserves hidden pixels.
- [ ] Zoom, pan or jump to edges, rulers, and the minimap behave correctly at every canvas size.
- [ ] Select colors from both halves of a 16-color palette; switching palettes preserves pixel indices.

## Sketches and preview

- [ ] With no saved sketches, Sketches shows the `+` tile and New Sketch works.
- [ ] Save updates the current sketch; Save As creates a separate sketch; both survive a restart.
- [ ] Open, duplicate, delete, and restore a sketch; deletion does not alter the current drawing unexpectedly.
- [ ] Preview and slideshow show the correct artwork, backgrounds, navigation, and auto-advance.
- [ ] A failed save or open reports an error and leaves the current drawing available.

## Palettes and export

- [ ] Built-in palettes apply; 4, 5–8, 9–16, and user filters show the expected results and empty messages.
- [ ] A valid 4–16-color `.hex` palette imports; an invalid file is rejected without changing artwork.
- [ ] Export both 128×128 and native-size PNGs; open the files and check colors and transparency.
- [ ] An unavailable export destination reports failure without marking the sketch as saved or exported.

## ADV hardware

- [ ] With a working SD card and existing sketches, Sketches opens without a visible flash.
- [ ] **Known issue to retest:** with no SD card, opening Sketches shows `SD: Not ready` and does not flash or briefly show part of the previous screen.
- [ ] If available, repeat the previous check with an SD card whose `sketches/` folder is empty; note whether the flash follows the missing card or the empty list.
- [ ] SD folders are created; sketches, custom palettes, and PNG exports are readable after reinserting the card into a computer.
- [ ] Brightness, shake undo, indicator LED, and RGB matrix controls work when the hardware is attached.

## Zero hardware

- [ ] Package installs and launches from HOME; Escape returns through views and a three-second hold returns HOME.
- [ ] Keyboard shortcuts match the [Zero guide](../platform_zero/README.md#controls); display brightness and warmth respond and persist.
- [ ] Battery and charging indicators reflect device state; shake undo appears only on supported hardware.
- [ ] Lospec browse shows `PALETTES>LOSPEC (count)`; filters, import by name or URL, and saved-palette exclusion work.
- [ ] Offline or failed Lospec refresh keeps cached palettes available; empty cache and no matches show distinct messages.
- [ ] Imported palettes remain available offline and can be deleted from local Palettes without changing saved artwork.

## Final release pass

- [ ] Run the relevant build and automated checks from the [development guide](development.md).
- [ ] Repeat the first-launch, save/open, export, and quit flows on the packaged build, not only a simulator.
- [ ] Check release assets and installation instructions for the intended platform and version.
- [ ] Record unresolved failures with device, firmware or package version, reproduction steps, and a photo or short video.
