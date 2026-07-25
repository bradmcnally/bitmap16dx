#pragma once

#include <cstddef>
#include <cstdint>

namespace bitmap16 {

enum class TextAlign : uint8_t {
  Left,
  Center,
  Right,
};

class Canvas {
 public:
  Canvas() = default;
  ~Canvas();

  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  bool create(int width, int height);
  void release();
  bool isValid() const { return pixels_ != nullptr; }

  int width() const { return width_; }
  int height() const { return height_; }
  uint16_t* pixels() { return pixels_; }
  const uint16_t* pixels() const { return pixels_; }

  void fillScreen(uint16_t color);
  void drawPixel(int x, int y, uint16_t color);
  uint16_t readPixel(int x, int y, uint16_t fallback = 0) const;
  void fillRect(int x, int y, int width, int height, uint16_t color);
  void drawRect(int x, int y, int width, int height, uint16_t color);
  void drawFastHLine(int x, int y, int width, uint16_t color);
  void drawFastVLine(int x, int y, int height, uint16_t color);
  void drawLine(int x0, int y0, int x1, int y1, uint16_t color);
  void pushImage(
      int x,
      int y,
      int width,
      int height,
      const uint16_t* image,
      bool swapBytes = false);

  void setTextColor(uint16_t color) { textColor_ = color; }
  void setTextSize(uint8_t size) { textSize_ = size == 0 ? 1 : size; }
  void setTextAlign(TextAlign align) { textAlign_ = align; }
  int textWidth(const char* text) const;
  void drawString(const char* text, int x, int y);

 private:
  int width_ = 0;
  int height_ = 0;
  uint16_t* pixels_ = nullptr;
  uint16_t textColor_ = 0xffff;
  uint8_t textSize_ = 1;
  TextAlign textAlign_ = TextAlign::Left;
};

}  // namespace bitmap16
