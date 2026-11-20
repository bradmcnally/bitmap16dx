# BitMap16 DX

A pixel-art sketchbook for drawing with indexed colors.

## Language

**Sketch**:
A drawing with its own palette and canvas size. Artwork outside the current
canvas size remains part of the sketch when the canvas shrinks.

**Canvas**:
The visible, editable area of a sketch at its current size.

**Palette**:
The ordered collection of colors used to display and draw a sketch. Changing
palettes preserves the artwork's color indices.

**Edit**:
A change to a sketch's pixels, palette, or canvas size. An action that leaves
the sketch unchanged is not an edit.

**Editor**:
The current sketch together with its drawing cursor, selected color, and
session undo/redo history.

**Palette browser**:
The place to browse built-in, saved, and downloadable palettes, and choose one
for the current sketch.

**Palette import**:
Saving a downloaded palette for future use and applying it to the current
sketch. An unsuccessful save leaves the sketch's palette unchanged.

**Sketch replacement**:
Creating a new sketch or opening a saved sketch in place of the current drawing.
Unsaved work can be saved, discarded, or kept by cancelling the replacement.

**Sketch workflow**:
The decisions involved in saving the current sketch, replacing it, or quitting.
A failed save or replacement leaves the current drawing available.
