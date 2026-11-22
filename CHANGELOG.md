# Changelog

All notable user-facing changes to BitMap16 DX are documented here.

The v0.8.0 release artifact targets the M5Stack Cardputer ADV. Version 0.9.0
adds a Cardputer Zero package.

## [0.9.1] — Unreleased

### Added

- Added Zero USB keyboard support with arrow-key navigation, Space to draw, and adaptive Help mappings. USB F/Z/X/C remain letter keys.

### Fixed

- Restored previewing imported Lospec palettes before saving and applying them.
- Fixed Zero Sym-key input, including hyphens, periods, and slashes in palette names and URLs.
- Fixed the save-changes prompt flashing when opening or moving between buttons.
- Fixed the import-preview cartridge appearing without its entrance animation.
- Fixed ADV firmware compilation with newer FastLED versions.
- Fixed periodic screensaver stutters while a USB keyboard is connected.
- Clarified built-in keyboard shortcuts in USB Help.

This release retains the v0.9.0 audio-free build. Music, sound effects, Audio
settings, and system-volume controls are reserved for a later release.

## [0.9.0] — Cardputer Zero

### Added

- Added a native Cardputer Zero app!
- Added Lospec feed browsing and palette import by name or URL on Zero and the desktop builds.
- Expanded undo and redo to 16 steps.
- Added mouse support to the desktop builds.

### Changed

- Updated `.hex` and Lospec palette imports to accept any number of colors from 4 through 16. Palette filters now group 4, 5–8, and 9–16 colors.
- Reorganized Help into clearer sections and revised its labels.
- Refined palette, save, and confirmation dialogs.
- Reduced startup splash screen time.
- Added a setting to hide the Zero battery icon. Charging appears in the same status area when the device reports it is charging.

### Fixed

- Fixed sketch duplication, deletion, and restoration replacing the open drawing. Restoring a deleted sketch on ADV now preserves canvas undo history.
- Fixed battery indicator jitter and Help navigation and label clipping.
- Fixed palette selection after switching from a larger palette to a smaller one, without changing existing artwork's pixel indices.

## [0.8.0] — Blast Processing

Bitmap16DX has Blast Processing! This update pushes past the original 8×8 and 16×16 workspace with 32×32 artwork, zoom, 

### Added

- Added 32×32 drawing support throughout editing, saving, loading, thumbnails, palettes, preview, and RGB Matrix viewport mirroring.
- Added canvas zoom for 16×16 and 32×32 artwork.
- Added a 32×32 minimap:
  - 16×16 artwork renders at an exact 2× scale.
  - 32×32 artwork renders at an exact 1× scale.
- Added Fn + Arrow for edge navigation while zoomed.
- Added option for StampS3A LED to display the currently selected color or low battery indicator.
- Added Ctrl as a second ADV draw button:
  - Tap Ctrl to place a pixel.
  - Hold Ctrl with the arrow keys to draw continuously.
- Added Fn + F Erase Fill for flood-filling a connected region with transparency.
- Added single step redo with Fn + Z.
- Added zoom in Preview.
- Added optional save warnings, enabled by default.
  - Save, Discard, or Cancel before creating/opening another document.
  - Warnings appear only after artwork-changing actions.
- Added Fn + S to duplicate the focused artwork in Sketches menu.
- Reworked the RGB Matrix settings page with:
  - On/off control
  - One- or four-unit layout
  - Rotation
  - Brightness
- Added viewport-aware RGB Matrix output for zoomed 16×16 and 32×32 artwork.
- Added a StampS3A indicator LED settings page:
  - On/off control for Palette Color
  - On/off control for Low Battery indicator (10%)

### Changed

- Rulers are now centered on the visible canvas viewport, so they remain visible while zooming and panning.
- The RGB Matrix parent setting now displays `ON >` or `OFF >` to communicate both its current state and nested navigation.
- Sketch files now support up to 32×32 indexed pixels.
- Saving an existing sketch gives it the newest sequence and brings it to the top of Sketches.
- Opening Sketches after a save focuses the newly saved item.
- Sketch thumbnails use integer scaling and preserve transparent pixels.
- Preview defaults to the current theme background instead of black.
- Zoom, Preview, Saved, Fill, Erase Fill, Move, and other relevant operations provide temporary status and motion feedback.
- The ADV Help screen documents Erase Fill, redo, and current controls.

### Fixed

- Fixed palettes remaining collapsed after switching from a smaller palette
  back to a larger one.
- Fixed the painting focus overlay remaining visible while using Move.
- Fixed L + Enter drawing a pixel while toggling the RGB Matrix.
- Fixed status label size inheriting the wrong size when exiting from settings screen. 
