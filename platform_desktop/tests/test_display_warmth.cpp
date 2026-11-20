#include <cstdlib>
#include <iostream>
#include <vector>

#include "core/display_warmth.h"
#include "core/settings_view.h"

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

}  // namespace

int main() {
  // Exercise every possible RGB565 color, including darks and pure primaries.
  std::vector<uint16_t> source(65536);
  for (std::size_t index = 0; index < source.size(); ++index) {
    source[index] = static_cast<uint16_t>(index);
  }
  bitmap16::DisplayWarmth filter;
  require(filter.apply(source.data(), source.size(), 0) == source.data(),
          "Neutral must return the original, unmodified frame");
  require(filter.apply(source.data(), source.size(), 255) == source.data(),
          "Invalid warmth must fall back to neutral");
  std::vector<uint16_t> previous = source;
  constexpr uint16_t white[] = {0xffff, 0xffde, 0xffbc, 0xff9a};
  for (uint8_t level = 1; level <= 3; ++level) {
    const uint16_t* output = filter.apply(source.data(), source.size(), level);
    require(output != source.data(), "Correction needs a separate output frame");
    require(output[0] == 0, "Warmth must preserve black");
    require(output[65535] == white[level], "Unexpected white-point correction");
    for (std::size_t index = 0; index < source.size(); ++index) {
      require(source[index] == index, "Warmth mutated the artwork/export canvas");
      require((output[index] & 0xf800) == (source[index] & 0xf800),
              "Warmth must preserve the red channel");
      require((output[index] & 0x07e0) <= (previous[index] & 0x07e0),
              "Increasing warmth must not increase green");
      require((output[index] & 0x001f) <= (previous[index] & 0x001f),
              "Increasing warmth must not increase blue");
    }
    previous.assign(output, output + source.size());
  }
  require(filter.apply(source.data(), source.size(), 0) == source.data(),
          "Returning to neutral must immediately bypass the filter");
  require(filter.apply(source.data(), source.size(), 1)[65535] == white[1],
          "Changing back to mild must refresh the cached channel tables");

  const int count = bitmap16::SettingsView::itemCount(false, false, false);
  for (int row = 0; row < count; ++row) {
    bitmap16::Settings settings;
    bitmap16::SettingsView::State state;
    state.cursor = row;
    require(bitmap16::SettingsView::activate(state, settings, false, false, false) !=
                bitmap16::SettingsView::Action::DisplayWarmthChanged,
            "Production Settings must hide beta Warmth");
  }
  bitmap16::Settings settings;
  settings.displayWarmth = 255;
  require(bitmap16::normalizeSettings(settings).displayWarmth == 0,
          "Invalid persisted warmth must reset to neutral");
  std::cout << "Display warmth: RGB565 filtering, hidden production setting, and persisted-value reset passed.\n";
  return 0;
}
