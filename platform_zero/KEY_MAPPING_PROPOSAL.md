# BitMap16 DX Cardputer Zero key proposal

This document proposes the physical keyboard mapping for the Cardputer Zero
port of BitMap16 DX. The existing Cardputer ADV controls remain unchanged.

## Design rules

- Bare `F`, `Z`, `X`, and `C` act as Up, Left, Down, and
  Right, as recommended by the Cardputer Zero key specification.
- The Zero firmware's `Fn+F`, `Fn+Z`, `Fn+X`, and `Fn+C` arrow combinations
  remain supported for normal navigation.
- `Shift+F/Z/X/C` jumps to an artwork edge when zoomed.
- `Ctrl` is the application-command modifier. Pressing `Ctrl` by itself does
  not draw on Zero.
- `1` through `8` select colors 1 through 8. `Fn+1` through `Fn+8`, received
  by the application as `F1` through `F8`, select colors 9 through 16.
- `Fn+Backspace`, received as Delete, replaces the ADV's G0 action. Its result
  depends on the current view.

## Canvas

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Move up | `Up` | `F` or `Fn+F` |
| Move down | `Down` | `X` or `Fn+X` |
| Move left | `Left` | `Z` or `Fn+Z` |
| Move right | `Right` | `C` or `Fn+C` |
| Jump to top edge | `Fn+Up` | `Shift+F` |
| Jump to bottom edge | `Fn+Down` | `Shift+X` |
| Jump to left edge | `Fn+Left` | `Shift+Z` |
| Jump to right edge | `Fn+Right` | `Shift+C` |
| Draw pixel | OK/Enter or Ctrl | Enter or space |
| Erase pixel | Backspace/Delete | Backspace |
| Clear canvas | G0 | `Fn+Backspace` |
| Fill | `F` | `Ctrl+F` |
| Erase fill | `Fn+F` | `E` |
| Undo | `Z` | `Ctrl+Z` |
| Redo | `Fn+Z` | `Ctrl+Y` |
| Cycle to next color | `C` | `I` |
| Select colors 1-8 | `1`-`8` | `1`-`8` |
| Select colors 9-16 | `Fn+1`-`Fn+8` | `Fn+1`-`Fn+8` |
| Save | `S` | `S` |
| Save as a new sketch | `Fn+S` | `Ctrl+S` |
| Export scaled PNG | `X` | `Ctrl+X` |
| Export logical-size PNG | `Fn+X` | `Ctrl+L` |
| Change grid size | `G` | `G` |
| Toggle rulers | `R` | `R` |
| Zoom in/out | `+` / `-` | `+` / `-` |
| Move artwork | `M` + arrow | `M` + `F/Z/X/C` |
| Open/close Help | `H` or escape | `H` or escape |
| Open/close Settings | `T` | `T` |
| Open/close Preview | `V` | `V` |
| Open/close Palette | `P` | `P` |
| Open/close Sketches | `O` | `O` |
| Open charging display | `Fn+B` | `Ctrl+B` |
| Adjust display brightness | `B` + `+/-` | `B` + `+/-` |
| Toggle RGB matrix | `L` + Enter | `L` + Enter |
| Adjust RGB matrix brightness | `L` + `+/-` | `L` + `+/-` |

`M+F/Z/X/C` is the only Zero artwork-movement form. Combining `M` with a
firmware Fn-arrow would require three held keys and is not supported.

## Sketches

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Navigate | Arrow keys | `F/Z/X/C` or `Fn+F/Z/X/C` |
| Activate selected slot | OK/Enter | Enter |
| Delete selected sketch | G0 | `Fn+Backspace` |
| Restore most recently deleted sketch | `Z` | `Ctrl+Z` |
| Duplicate selected sketch | `Fn+S` | `Ctrl+S` |
| Open slideshow | `V` | `V` |
| Close Sketches | `O` or Escape | `O` or Escape |

## Unsaved-changes prompt

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Save and continue | `S` | `S` |
| Discard and continue | `D` | `D` |
| Cancel | Escape | Escape |

## Palette

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Previous palette | Left | `Z` or `Fn+Z` |
| Next palette | Right | `C` or `Fn+C` |
| Select palette | OK/Enter | Enter |
| Toggle 4-color filter | `4` | `4` |
| Toggle 8-color filter | `8` | `8` |
| Toggle 16-color filter | `1` | `1` |
| Toggle user-palette filter | `U` | `U` |
| Clear all filters | `0` | `0` |
| Close Palette | `P` or Escape | `P` or Escape |

## Preview and slideshow

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Previous sketch in slideshow | Left | `Z` or `Fn+Z` |
| Next sketch in slideshow | Right | `C` or `Fn+C` |
| Toggle slideshow auto-advance | Space | Space |
| Select black background | `1` | `1` |
| Select white background | `2` | `2` |
| Select light-gray background | `3` | `3` |
| Select dark-gray background | `4` | `4` |
| Zoom in/out | `+` / `-` | `+` / `-` |
| Adjust display brightness | `B` + `+/-` | `B` + `+/-` |
| Close Preview | `V` or Escape | `V` or Escape |

## Settings

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Move up | Up | `F` or `Fn+F` |
| Move down | Down | `X` or `Fn+X` |
| Change or activate setting | Left, Right, OK/Enter, or Space | `Z`, `C`, Enter, or Space |
| Return from submenu | Escape | Escape |
| Close Settings | Escape | Escape |

## Help and charging display

| Function | Cardputer ADV keys | Cardputer Zero proposal |
|---|---|---|
| Scroll Help up | Up | `F` or `Fn+F` |
| Scroll Help down | Down | `X` or `Fn+X` |
| Close Help | `H` or Escape | `H` or Escape |
| Exit charging display | Any key | Any key |