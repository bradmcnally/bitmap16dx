# BitMap16 DX — Blast Processing Update

## ADV Release Test Checklist

Test with a freshly built ADV firmware and an SD card containing several
8×8, 16×16, and 32×32 `.dat` sketches. Include palettes with 4, 8, and 16
colors.

Record the firmware version, device, SD card, and result:

- Firmware:
- Device:
- SD card:
- Tester:
- Date:
- Overall result: [ ] Pass [ ] Fail

## 1. Build, install, and boot

- [ ] `pio run -e m5stack-cardputer` completes successfully.
- [ ] Firmware installs and boots without resetting or hanging.
- [ ] Splash screen appears correctly.
- [ ] Version and BeepBot name appear in the expected positions.
- [ ] App opens to a new blank sketch, not the previously opened sketch.
- [ ] Default grid size from Settings is respected.
- [ ] No previous-screen artifacts remain after the first redraw.

## 2. Basic drawing

Repeat the drawing checks on 8×8, 16×16, and 32×32 canvases.

- [ ] Enter draws one pixel.
- [ ] Holding Enter while pressing arrows draws a continuous line.
- [ ] Ctrl draws one pixel.
- [ ] Holding Ctrl while pressing arrows draws a continuous line.
- [ ] Backspace erases one pixel.
- [ ] Holding Backspace while pressing arrows erases continuously.
- [ ] Fill fills the expected four-way connected area.
- [ ] Fn + F fills the connected area with transparency.
- [ ] Filling an empty 32×32 canvas does not crash.
- [ ] Draw, erase, and fill icons animate when pressed.
- [ ] Pressed icons move without changing color.
- [ ] Tool icons remain aligned close to the canvas.
- [ ] Colors 1–8 select correctly.
- [ ] Fn + 1–8 selects colors 9–16.
- [ ] Cycle Color wraps through the current palette.
- [ ] Empty-pixel cursor overlay looks neutral rather than warm.

## 3. Undo, redo, move, and grid

- [ ] Z undoes the latest edit.
- [ ] Fn + Z redoes the latest undone edit.
- [ ] Shake undo works when enabled.
- [ ] Shake undo does nothing when disabled.
- [ ] Holding M shows the move cursor.
- [ ] The painting focus pixel is hidden while M is held.
- [ ] Arrow keys move the artwork while M is held.
- [ ] One move gesture creates one undo step.
- [ ] Undo restores the artwork after moving it.
- [ ] Grid-size changes preserve existing artwork correctly.
- [ ] Starting a new sketch resets canvas zoom and viewport position.

## 4. Canvas zoom and navigation

Test 16×16 and 32×32 artwork.

- [ ] Plus zooms in by integer levels.
- [ ] Minus zooms out by integer levels.
- [ ] A temporary `ZOOM: nX` status appears after changing zoom.
- [ ] Cursor movement scrolls the viewport when it reaches an edge.
- [ ] Cursor remains above artwork, rulers, and grid overlays.
- [ ] Fn + Up jumps to the top edge of the artwork.
- [ ] Fn + Down jumps to the bottom edge of the artwork.
- [ ] Fn + Left jumps to the left edge of the artwork.
- [ ] Fn + Right jumps to the right edge of the artwork.
- [ ] Edge jumps do not draw, erase, or move artwork.
- [ ] The minimap is hidden for 8×8 artwork.
- [ ] The 16×16 minimap is 32×32 and renders pixels at 2×.
- [ ] The 32×32 minimap is 32×32 and renders pixels at 1×.
- [ ] Minimap artwork is integer scaled with no missing or blended pixels.
- [ ] Minimap viewport indicator matches the visible canvas region.
- [ ] Indicator remains visible over artwork and is not cropped by corners.
- [ ] Minimap shadow and corner cuts match the rest of the UI.

## 5. Palettes

- [ ] Palette menu opens from the canvas.
- [ ] The carousel remembers the currently applied palette.
- [ ] Left and right arrows browse the carousel.
- [ ] Enter starts the cartridge insertion animation.
- [ ] Cartridge reaches and visually inserts into the slot.
- [ ] Animation has clear anticipation, acceleration, and weight.
- [ ] Palette applies after the animation completes.
- [ ] Palette menu dismisses after applying.
- [ ] 4-, 8-, and 16-color palettes render correctly.
- [ ] ADV palette colors remain right-aligned at the screen edge.
- [ ] Expanding from a smaller palette restores the larger palette indices.
- [ ] Thumbnail colors remain opaque after palette changes.
- [ ] Applying a palette marks the document as changed.

## 6. Save and save warnings

- [ ] S saves the current sketch.
- [ ] Fn + S saves the canvas as a new sketch.
- [ ] `Saved` status appears after a successful save.
- [ ] Saving places the sketch at the top of Sketches.
- [ ] Opening Sketches after saving focuses the saved sketch at the top.
- [ ] Save warnings are ON by default after a clean settings reset.
- [ ] `Save warnings` can be toggled ON and OFF in Settings.
- [ ] The setting persists after restart.
- [ ] A new blank sketch does not warn before it has been edited.
- [ ] Cursor movement and zoom do not trigger a warning.
- [ ] Drawing, erasing, filling, moving, undo/redo, grid changes, and palette
      changes trigger a warning.
- [ ] Selecting New with unsaved changes shows the warning.
- [ ] Opening another sketch with unsaved changes shows the warning.
- [ ] S in the warning saves and then completes the requested action.
- [ ] D in the warning discards and completes the requested action.
- [ ] Esc closes the warning and keeps the current sketch open.
- [ ] No warning appears when Save warnings are OFF.
- [ ] No warning appears immediately after a successful save.

## 7. Sketches screen

- [ ] Sketches opens without a long blank pause.
- [ ] Visible thumbnails load progressively.
- [ ] Scrolling remains responsive while thumbnails load.
- [ ] Reopening Sketches reuses cached thumbnails.
- [ ] Repeatedly opening and closing Sketches does not crash the ADV.
- [ ] 8×8, 16×16, and 32×32 thumbnails render correctly.
- [ ] Thumbnail artwork is centered in the 48×48 ADV tile.
- [ ] Transparent pixels use the expected background.
- [ ] Focus selector animates and uses black for its dark color.
- [ ] Scrolling does not become slower after extended navigation.
- [ ] New creates a blank sketch at the configured default size.
- [ ] Enter opens the focused sketch.
- [ ] Delete removes the focused sketch.
- [ ] Deleting a sketch does not replace or modify the open canvas document.
- [ ] Z restores the most recently deleted sketch.
- [ ] Restoring a deletion does not consume or overwrite Canvas undo history.
- [ ] V previews the focused sketch.
- [ ] Fn + S duplicates the focused sketch.
- [ ] Duplicate receives a new file and appears at the top.
- [ ] Focus moves to the new duplicate.
- [ ] Duplicating does not replace the currently active canvas document.
- [ ] Files copied externally to the SD card appear after restarting the app.

## 8. Preview

Repeat with 8×8, 16×16, and 32×32 artwork.

- [ ] Preview opens with a temporary `Preview` label.
- [ ] Label appears in Preview, not on the canvas.
- [ ] Preview initially uses the artwork’s logical size.
- [ ] Plus zooms in by one integer level.
- [ ] Minus zooms out by one integer level.
- [ ] Zoom label shows the current integer scale.
- [ ] Preview and zoom labels are left-aligned.
- [ ] Preview background defaults to the current theme background.
- [ ] Background color controls work.
- [ ] Preview exits to the correct previous screen.
- [ ] Gallery preview navigates among saved sketches correctly.

## 9. RGB Matrix

Test one 8×8 unit. If available, repeat with the four-unit 16×16 layout.

- [ ] Settings shows `RGB matrix` with `ON >` or `OFF >`.
- [ ] Enter opens the nested RGB Matrix page.
- [ ] Esc returns to the main Settings page.
- [ ] Enabled toggles the matrix on and off.
- [ ] L + Enter toggles the matrix without drawing a canvas pixel.
- [ ] Layout switches between 1 unit and 4 units.
- [ ] Rotation cycles through 0°, 90°, 180°, and 270°.
- [ ] Brightness changes immediately from the Settings page.
- [ ] L + Plus increases brightness.
- [ ] L + Minus decreases brightness.
- [ ] Brightness changes do not scramble, rotate, shift, or recolor the image.
- [ ] Brightness persists after restart.
- [ ] An 8×8 sketch maps correctly to one unit.
- [ ] An 8×8 sketch scales correctly across four units.
- [ ] A 16×16 sketch maps correctly across four units.
- [ ] A zoomed 16×16 or 32×32 canvas displays the visible viewport.
- [ ] Moving the cursor/viewport updates the matrix region correctly.
- [ ] Preview displays clean artwork without the cursor.

## 10. Settings and persistence

- [ ] Theme changes immediately and persists.
- [ ] Default grid cycles through 8, 16, and 32 and persists.
- [ ] Export format toggles and persists.
- [ ] Shake undo toggles and persists.
- [ ] Save warnings toggle and persist.
- [ ] RGB Matrix submenu values persist.
- [ ] Settings rows scroll without clipping.
- [ ] Selected and unselected values remain legible in both themes.
- [ ] Battery indicator stays visually stable while the canvas redraws.
- [ ] Battery percentage refreshes without requiring another canvas action.

## 11. Stability pass

- [ ] Draw continuously for at least two minutes without a crash.
- [ ] Fill a 32×32 canvas several times.
- [ ] Open and close Sketches at least 20 times.
- [ ] Scroll through the complete Sketches list several times.
- [ ] Open and apply palettes repeatedly.
- [ ] Zoom and pan across all corners of a 32×32 sketch.
- [ ] Save, duplicate, open, and delete several sketches in succession.
- [ ] Toggle and adjust the RGB Matrix repeatedly.
- [ ] Put the device through one restart and confirm saved settings/files.
- [ ] No unexplained resets, freezes, corrupted thumbnails, or display artifacts.

## 12. Release sign-off

- [ ] No blocking defects remain.
- [ ] All failures are documented with reproduction steps.
- [ ] Existing 8×8 and 16×16 sketches remain readable.
- [ ] New 32×32 sketches save and reopen without data loss.
- [ ] Release firmware version is correct.
- [ ] Release notes mention 32×32 artwork and save-warning behavior.
- [ ] Final ADV firmware artifact is archived.

## Failure notes

For each failure, record:

- Checklist item:
- Grid size:
- Sketch filename:
- Exact input sequence:
- Expected result:
- Actual result:
- Reproducible: [ ] Always [ ] Sometimes [ ] Once
- Photo/video/log:
