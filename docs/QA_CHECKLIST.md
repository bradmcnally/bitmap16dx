# BitMap16 DX QA checklist

Use a fresh install or isolated data folder for the first-run checks, then repeat the persistence checks with existing sketches. Record the build, device, date, and any failed step.

**Build:** __________  **Device:** __________  **Date:** __________

## First launch and navigation

- [ ] Menu, dialog-focus, and palette-selection changes play varied quiet navigation ticks. Repeated boundary presses stay silent; navigation does not interrupt cartridge cues or background music.
- [ ] App opens to a new blank canvas after the splash screen (about 1.5 seconds); Escape skips the splash.
- [ ] Canvas, Sketches, Palettes, Preview, Settings, and Help open and return to the expected view.
- [ ] Light and dark themes render legibly; settings survive a restart.
- [ ] Unsaved-changes prompts offer the correct save, discard, and cancel behavior.

## Drawing

- [ ] Changing the selected color with number keys, I, or controller swatch navigation plays varied quiet ticks. Re-selecting the current color and moving the drawing cursor stay silent.
- [ ] Draw and erase single pixels and continuous lines; fill and erase fill stay within the selected region.
- [ ] Move artwork, clear the canvas, undo, and redo; undo history resets when opening or creating a sketch.
- [ ] After undoing a stroke to an empty canvas, clear, erase fill, or move without changing pixels; redo still restores the stroke.
- [ ] Resize among 8×8, 16×16, and 32×32; shrinking and growing preserves hidden pixels.
- [ ] Zoom, pan or jump to edges, rulers, and the minimap behave correctly at every canvas size.
- [ ] Select colors from both halves of a 16-color palette; switching palettes preserves pixel indices.
- [ ] Select color 12, apply a four-color palette, and draw; color 4 is selected while existing artwork keeps its pixel indices.

## Sketches and preview

- [ ] With no saved sketches, Sketches shows the `+` tile and New Sketch works.
- [ ] Save updates the current sketch; Save As creates a separate sketch; both survive a restart.
- [ ] Open, duplicate, delete, and restore a sketch; on ADV, G0 and Delete/Backspace open the confirmation, and OK/Enter or G0 confirms the selected option. Deletion does not alter the current drawing unexpectedly.
- [ ] Preview and slideshow show the correct artwork, backgrounds, navigation, and auto-advance.
- [ ] A failed save or open reports an error and leaves the current drawing available.
- [ ] Select an older sketch while the current one has unsaved edits, choose Save, and confirm the originally selected sketch opens after the list reorders.
- [ ] Discard followed by a failed open preserves the current drawing and its unsaved warning; Cancel preserves undo history.
- [ ] Delete the current saved sketch and verify Save still works; restoring an older deletion after New or Open does not change the current sketch's save destination.

## Palettes and export

- [ ] Open Settings > Audio. Toggle Sound effects off and verify navigation, swatches, cartridge, and menu sounds stop while music continues. Restart to check both toggles are saved; re-enable effects.
- [ ] Adjust Sound effects volume and Music volume with left/right. Each clamps to 0–100% in 10% steps and changes only its own mixer gain; system volume stays fixed. At 0%, effects are silent or music continues silently without pausing. Restore levels, restart, and confirm both percentages are saved. Master volume keys still change overall output.
- [ ] Open/close Palettes, Sketches, Settings, and Help: enable_02 plays on open and disable_02 on close. Returning from nested Help plays only the close cue; applying a cartridge keeps its success cue.
- [ ] Open an existing sketch from Sketches: enable_04a replaces the dismiss sound and respects Sound effects volume and Sound effects on/off. Backing out still plays disable_02. Failed opens and canceling the unsaved-changes prompt stay in Sketches without the sketch-open cue.
- [ ] Successful Save, Save As, and Save in the unsaved-changes prompt play success_melodic_03b once, following Sound effects volume and Sound effects on/off. Failed or canceled saves have no success cue. Saving before opening another sketch preserves both the save and open sounds.
- [ ] On Zero, plain Q does not quit; Fn+Q still controls music, and holding Escape for three seconds returns HOME.
- [ ] Enable Music, visit every view and modal dialog, and confirm playback continues. Fn+Q pauses/resumes at the same position; Fn+W/E skips tracks without activating other shortcuts.
- [ ] On Steam Deck, L3 pauses/resumes music once per press in Canvas, Palettes, Settings, Help, Preview, Sketches, and modal dialogs. Releasing L3 does not toggle again, resize, or open Import. L4 still resizes Canvas and opens Import in Palettes; Help and palette hints show L4 for those actions.
- [ ] Let the last music track finish and confirm the playlist repeats. Confirm tracks display BGM01, BGM02, etc. Skip while paused, test music volume independently of the cartridge sound, and restart to verify saved preferences. Repeat media-key checks on desktop/Steam Deck.
- [ ] Applying a palette plays transition_soft_02b while sliding and success_melodic_01b once at the end of insertion. The success sound must not repeat during the final hold; browsing, opening import previews, canceled selections, and failed imports stay silent. Check that background music continues and animation remains smooth.
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

- [ ] Fn+S/D lowers/raises master system volume in 5% steps on every view and inside dialogs. Check the percentage overlay, 0–100% limits, hold-repeat, and USB volume keys; HOME reflects the new level and changes made in HOME carry into the app.
- [ ] With the switch on HUB, connect a USB keyboard before launch and during use; arrows navigate and Enter+Arrow or Space+Arrow draws continuously. Backspace/Delete+Arrow erases, while built-in Fn+Backspace still clears.
- [ ] USB Ctrl+Z undoes; Ctrl+Shift+Z and Ctrl+Y redo. Shift+Arrow jumps when zoomed, M+Arrow moves artwork, F1–F8 selects colors 9–16, and punctuation works in Lospec import.
- [ ] Use both keyboards together, unplug USB while holding Draw, a direction, a modifier, or Escape, and reconnect. No keys remain stuck, the app stays open, and the built-in keyboard still works.
- [ ] USB F/Z/X/C do not navigate Canvas, Settings, Help, Palettes, Sketches, resize, or confirmation dialogs; built-in F/Z/X/C still navigate, and Ctrl+F/Z/X retain their commands.
- [ ] While Help is open, connect and disconnect USB and alternate between the keyboards. HELP USB shows Arrows, M arrows, F1–F8, and USB shortcuts; built-in input restores the device mappings without leaving Help.
- [ ] Package installs and launches from HOME; Escape returns through views and a three-second hold returns HOME.
- [ ] Keyboard shortcuts match the [Zero guide](../platform_zero/README.md#controls); display brightness responds and persists, and the beta Warmth setting is hidden.
- [ ] Battery and charging indicators reflect device state; shake undo appears only on supported hardware.
- [ ] Lospec browse shows `PALETTES>LOSPEC (count)`; filters, import by name or URL, and saved-palette exclusion work.
- [ ] Offline or failed Lospec refresh keeps cached palettes available; empty cache and no matches show distinct messages.
- [ ] Imported palettes remain available offline and can be deleted from local Palettes without changing saved artwork.

## Final release pass

- [ ] Run the relevant build and automated checks from the [development guide](development.md).
- [ ] Repeat the first-launch, save/open, export, and quit flows on the packaged build, not only a simulator.
- [ ] Check release assets and installation instructions for the intended platform and version.
- [ ] Record unresolved failures with device, firmware or package version, reproduction steps, and a photo or short video.
