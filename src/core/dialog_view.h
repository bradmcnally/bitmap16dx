#pragma once

#include "core/canvas.h"
#include "core/sketch.h"

namespace bitmap16 {
namespace DialogView {

struct Theme {
  uint16_t background;
  uint16_t text;
  uint16_t iconDark;
  uint16_t iconLight;
  bool dark;
};
struct Frame { int x, y, width, height; };
enum class Icon { Save, SkullCalm, SkullDelete, Resize };

Frame drawFrame(Canvas& canvas, const Theme& theme, const char* title,
                int height, int titleOffset = 44);
void drawOption(Canvas& canvas, const Theme& theme, const char* label,
                int x, int y, bool selected, int minimumWidth = 64);
int optionX(const Canvas& canvas, int index, int count);
void drawIcon(Canvas& canvas, int y, Icon icon, const Theme& theme);
void drawSave(Canvas& canvas, const Theme& theme, int selected);
void drawDelete(Canvas& canvas, const Theme& theme, bool deleteSelected);
void drawResize(Canvas& canvas, const Sketch& sketch, const Theme& theme,
                uint8_t selectedSize, int actionFocus);

}  // namespace DialogView
}  // namespace bitmap16
