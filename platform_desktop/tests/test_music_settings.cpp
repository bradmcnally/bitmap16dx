#include "core/settings_view.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main() {
  using namespace bitmap16;
  const int standard = SettingsView::itemCount(false, false, false);
  const int withAudio = SettingsView::itemCount(false, false, false, false, false, false, true);
  require(withAudio == standard + 1, "Audio settings appear only on platforms that expose audio");
  Settings settings;
  require(settings.soundEffectsEnabled && !settings.musicEnabled && settings.musicVolume == 30, "Music is optional and starts quietly");
  SettingsView::State state;
  int audioRow = -1;
  for (int row = 0; row < withAudio; ++row) {
    state = {};
    state.cursor = row;
    Settings copy = settings;
    if (SettingsView::activate(state, copy, false, false, false, false, false, false, true) ==
        SettingsView::Action::AudioMenuRequested) {
      audioRow = row;
      break;
    }
  }
  require(audioRow >= 0 && state.page == SettingsView::Page::Audio &&
          state.parentCursor == audioRow && state.cursor == 0 && state.scrollOffset == 0,
          "Audio opens a separate page and remembers its parent row");
  int effectsRow = -1, enabledRow = -1, effectsVolumeRow = -1, musicVolumeRow = -1;
  for (int row = 0; row < 4; ++row) {
    state.cursor = row;
    Settings copy = settings;
    const auto action = SettingsView::activate(state, copy, false, false, false, false, false, false, true);
    if (action == SettingsView::Action::SoundEffectsChanged) effectsRow = row;
    if (action == SettingsView::Action::MusicEnabledChanged) enabledRow = row;
    if (action == SettingsView::Action::SoundEffectsVolumeChanged) effectsVolumeRow = row;
    if (action == SettingsView::Action::MusicVolumeChanged) musicVolumeRow = row;
    // Effects start at 100%, so increasing that row correctly does nothing.
    if (action == SettingsView::Action::None) {
      const auto decreased = SettingsView::activate(
          state, copy, false, false, false, false, false, false, true, -1);
      if (decreased == SettingsView::Action::SoundEffectsVolumeChanged) effectsVolumeRow = row;
    }
  }
  require(effectsRow >= 0 && enabledRow >= 0 && effectsVolumeRow >= 0 && musicVolumeRow >= 0,
          "Audio exposes independent on/off toggles and mix levels");
  require(!SettingsView::moveCursor(state, 1, false, false, false, false, false, false, true),
          "Audio ends after the toggles and mix levels, without track rows");
  state.cursor = effectsRow;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true);
  require(!settings.soundEffectsEnabled && !settings.musicEnabled,
          "Effects toggle leaves music unchanged");
  state.cursor = enabledRow;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true);
  require(settings.musicEnabled && !settings.soundEffectsEnabled,
          "Music toggle leaves effects unchanged");
  require(settings.musicVolume == 30, "Audio toggles preserve the existing music gain");
  state.cursor = effectsVolumeRow;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true, -1);
  require(settings.soundEffectsVolume == 90 && settings.musicVolume == 30 &&
          !settings.soundEffectsEnabled && settings.musicEnabled,
          "Effects mix changes independently without changing either on/off switch");
  settings.soundEffectsVolume = 5;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true, -1);
  require(settings.soundEffectsVolume == 0, "Effects mix clamps to zero");
  require(SettingsView::activate(state, settings, false, false, false, false, false, false, true, -1) ==
          SettingsView::Action::None, "Decreasing a zero mix leaves it at zero");
  state.cursor = musicVolumeRow;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true, -1);
  require(settings.musicVolume == 20 && settings.soundEffectsVolume == 0 && settings.musicEnabled,
          "Music mix decreases independently without pausing playback");
  settings.musicVolume = 95;
  SettingsView::activate(state, settings, false, false, false, false, false, false, true);
  require(settings.musicVolume == 100, "Music mix clamps to 100 percent");
  require(SettingsView::activate(state, settings, false, false, false, false, false, false, true) ==
          SettingsView::Action::None && settings.musicVolume == 100,
          "Increasing a full mix leaves it at 100 instead of wrapping to mute");
  settings.musicVolume = 255;
  settings.soundEffectsVolume = 255;
  require(normalizeSettings(settings).musicVolume == 100 &&
          normalizeSettings(settings).soundEffectsVolume == 100, "Persisted invalid mix levels are clamped");
}
