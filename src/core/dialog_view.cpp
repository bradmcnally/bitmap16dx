#include "core/dialog_view.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/canvas_view.h"
#include "core/dialog_artwork.h"

namespace bitmap16 {
namespace DialogView {

Frame drawFrame(
    bitmap16::Canvas& canvas, const Theme& theme,
    const char* title, int height, int titleOffset) {
  const int width = std::min(252, canvas.width() - 16);
  height = std::min(height, canvas.height() - 8);
  const Frame frame{(canvas.width() - width) / 2,
                          (canvas.height() - height) / 2, width, height};
  canvas.fillRect(frame.x, frame.y, width, height, theme.background);
  canvas.drawRect(frame.x, frame.y, width, height, theme.text);
  canvas.drawRect(frame.x + 1, frame.y + 1, width - 2, height - 2, theme.text);
  canvas.setTextSize(1);
  canvas.setTextAlign(bitmap16::TextAlign::Center);
  canvas.setTextColor(theme.text);
  canvas.drawString(title, canvas.width() / 2, frame.y + titleOffset);
  return frame;
}

constexpr int kDialogButtonTopPadding = 7;
constexpr int kDialogButtonHeight = 21;

void drawOption(
    bitmap16::Canvas& canvas, const Theme& theme,
    const char* label, int x, int y, bool selected, int minimumWidth) {
  const int width = std::max(minimumWidth, static_cast<int>(std::strlen(label)) * 6 + 12);
  if (selected) canvas.fillRect(x - width / 2, y - kDialogButtonTopPadding,
                                width, kDialogButtonHeight, theme.text);
  canvas.setTextColor(selected
      ? (theme.dark ? 0x0000 : 0xffff)
      : theme.text);
  canvas.setTextAlign(bitmap16::TextAlign::Center);
  canvas.drawString(label, x, y);
}

int optionX(const bitmap16::Canvas& canvas, int index, int count) {
  constexpr int buttonWidth = 64;
  constexpr int gap = 12;
  return canvas.width() / 2 + (2 * index - count + 1) * (buttonWidth + gap) / 2;
}

void drawIcon(bitmap16::Canvas& canvas, int y, Icon icon,
                    const Theme& theme) {
  const auto draw = [&](const auto& runs, int width, int height) {
    const int left = (canvas.width() - width) / 2;
    const int top = y + (24 - height) / 2;
    for (const auto& run : runs)
      canvas.fillRect(left + run.x, top + run.y, run.width, 1,
                      run.color == 0x2945 ? theme.iconDark : theme.iconLight);
  };
  if (icon == Icon::Save) {
    draw(DialogArtwork::SAVE, DialogArtwork::SAVE_WIDTH, DialogArtwork::SAVE_HEIGHT);
    return;
  }
  if (icon == Icon::SkullCalm) {
    draw(DialogArtwork::HAPPY, DialogArtwork::HAPPY_WIDTH, DialogArtwork::HAPPY_HEIGHT);
    return;
  }
  if (icon == Icon::SkullDelete) {
    draw(DialogArtwork::ANGRY, DialogArtwork::ANGRY_WIDTH, DialogArtwork::ANGRY_HEIGHT);
    return;
  }
  draw(DialogArtwork::RESIZE, DialogArtwork::RESIZE_WIDTH, DialogArtwork::RESIZE_HEIGHT);
}

void drawSave(
    bitmap16::Canvas& canvas,
    const Theme& theme, int selected) {
  const auto frame = drawFrame(canvas, theme, "KEEP CHANGES?", 96);
  drawIcon(canvas, frame.y + 12, Icon::Save, theme);
  constexpr const char* labels[] = {"CANCEL", "DISCARD", "SAVE"};
  for (int i = 0; i < 3; ++i) {
    drawOption(canvas, theme, labels[i],
                     optionX(canvas, i, 3),
                     frame.y + 68, selected == i);
  }
}

void drawResize(
    bitmap16::Canvas& canvas,
    const bitmap16::Sketch& sketch,
    const Theme& theme,
    uint8_t selectedSize, int actionFocus) {
  bool hidesPixels = false;
  if (selectedSize < sketch.gridSize) {
    const auto layout = bitmap16::CanvasView::layoutFor(
        canvas.width(), canvas.height(), sketch.gridSize);
    const int pixels = selectedSize * layout.cellSize;
    canvas.drawRect(layout.gridX, layout.gridY, pixels, pixels, 0x0000);
    canvas.drawRect(layout.gridX + 1, layout.gridY + 1,
                    pixels - 2, pixels - 2, 0xffff);
    for (int y = 0; y < sketch.gridSize && !hidesPixels; ++y) {
      for (int x = y < selectedSize ? selectedSize : 0;
           x < sketch.gridSize; ++x) {
        if (sketch.pixels[y][x] != 0) {
          hidesPixels = true;
          break;
        }
      }
    }
  }

  const bool compact = canvas.height() < 148;
  const auto frame = drawFrame(canvas, theme, "RESIZE CANVAS",
      hidesPixels ? (compact ? 128 : 140) : (compact ? 108 : 120), compact ? 36 : 44);
  const int panelY = frame.y;
  const int centerX = canvas.width() / 2;
  char label[40] = {};
  drawIcon(canvas, panelY + (compact ? 4 : 12), Icon::Resize, theme);

  const int sizesY = panelY + (compact ? 56 : 68);
  constexpr uint8_t sizes[] = {8, 16, 32};
  for (int index = 0; index < 3; ++index) {
    const uint8_t size = sizes[index];
    const int x = optionX(canvas, index, 3);
    std::snprintf(label, sizeof(label), "%s%ux%u",
                  size == sketch.gridSize ? "> " : "", size, size);
    drawOption(canvas, theme, label, x, sizesY,
                     actionFocus < 0 && size == selectedSize);
    if (actionFocus >= 0 && size == selectedSize) {
      canvas.drawRect(x - 32, sizesY - kDialogButtonTopPadding,
                      64, kDialogButtonHeight, theme.text);
    }
  }

  canvas.setTextColor(theme.text);
  if (hidesPixels) {
    std::snprintf(label, sizeof(label), "Outside %ux%u will be hidden",
                  selectedSize, selectedSize);
    canvas.drawString(label, centerX, panelY + (compact ? 82 : 94));
  }
  drawOption(canvas, theme, "CANCEL", optionX(canvas, 0, 2),
                   panelY + frame.height - 24, actionFocus == 0);
  drawOption(canvas, theme, "APPLY", optionX(canvas, 1, 2),
                   panelY + frame.height - 24, actionFocus == 1);

}

void drawDelete(Canvas& canvas, const Theme& theme, bool deleteSelected) {
  const auto frame = drawFrame(canvas, theme, "DELETE THIS SKETCH?", 96);
  drawIcon(canvas, frame.y + 12, deleteSelected ? Icon::SkullDelete : Icon::SkullCalm, theme);
  drawOption(canvas, theme, "CANCEL", optionX(canvas, 0, 2), frame.y + 68, !deleteSelected);
  drawOption(canvas, theme, "DELETE", optionX(canvas, 1, 2), frame.y + 68, deleteSelected);
}

}  // namespace DialogView
}  // namespace bitmap16
