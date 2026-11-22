#include "audio_player.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cmath>
#include <fstream>

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << ": " << SDL_GetError() << '\n'; std::exit(1); }
}
}

int main(int argc, char** argv) {
  const bool unavailable = argc > 1 && std::strcmp(argv[1], "--unavailable") == 0;
  SDL_setenv("SDL_AUDIODRIVER", unavailable ? "bitmap16-no-audio-driver" : "dummy", 1);
  {
    bitmap16::Desktop::AudioPlayer sounds;
    require(!sounds.playMenuOpen() && !sounds.playMenuClose(), "Menu playback before initialization is harmless");
    require(!sounds.playSketchOpen(), "Sketch playback before initialization is harmless");
    require(!sounds.playSketchSave(), "Save playback before initialization is harmless");
    require(!sounds.playNavigationTick(0), "Navigation before audio initialization is harmless");
    require(!sounds.playCartridgeSlide(), "Slide playback before initialization is harmless");
    require(!sounds.playCartridgeInsert(), "Playing before initialization is harmless");
    if (unavailable) {
      require(!sounds.open(), "Unavailable audio is reported without terminating the app");
      require(!sounds.playCartridgeInsert(), "An insertion remains harmless when audio is unavailable");
      require(!sounds.playCartridgeSlide(), "A slide remains harmless when audio is unavailable");
      require(!sounds.playNavigationTick(0), "Navigation is harmless when audio is unavailable");
      require(!sounds.playSketchOpen(), "Sketch playback is harmless when audio is unavailable");
      require(!sounds.playSketchSave(), "Save playback is harmless when audio is unavailable");
    } else {
      require(sounds.open(BITMAP16_MUSIC_TEST_DIR), "Embedded WAV loads and the output opens");
      require(sounds.playNavigationTick(0), "The first navigation tick plays immediately");
      Mix_Chunk* previousTick = Mix_GetChunk(1);
      require(previousTick != nullptr && Mix_VolumeChunk(previousTick, -1) == MIX_MAX_VOLUME / 2,
              "Navigation uses its own quieter sound channel");
      require(!sounds.playNavigationTick(99), "Rapid navigation is rate limited");
      for (Uint32 now = 100; now <= 1600; now += 100) {
        require(sounds.playNavigationTick(now), "Navigation resumes after its short cooldown");
        auto* tick = Mix_GetChunk(1);
        require(tick != nullptr && tick != previousTick,
                "Random navigation avoids immediately repeating the same sample");
        previousTick = tick;
      }
      require(sounds.trackCount() > 1 && !sounds.musicEnabled(), "Playlist is discovered but optional music starts off");
      sounds.setMusicVolume(30);
      require(sounds.setMusicEnabled(true) && sounds.trackIndex() == 0,
              "Play starts the first MP3 in filename order");
      require(sounds.trackName() == "BGM01", "First track has its BGM label");
      SDL_Delay(80);
      require(sounds.setMusicEnabled(false) && Mix_PausedMusic(), "Pause stops playback without unloading the track");
      const double pausedPosition = Mix_GetMusicPosition(nullptr);
      require(pausedPosition >= 0, "The MP3 decoder reports a resumable playback position");
      SDL_Delay(60);
      require(std::abs(Mix_GetMusicPosition(nullptr) - pausedPosition) < 0.005,
              "Paused music holds its position");
      require(sounds.setMusicEnabled(true) && !Mix_PausedMusic(), "Play resumes paused music");
      require(sounds.playCartridgeSlide() && Mix_PlayingMusic() && !Mix_PausedMusic(),
              "The slide effect plays over background music");
      require(sounds.playCartridgeInsert() && Mix_PlayingMusic() && !Mix_PausedMusic(),
              "Cartridge effects play over background music");
      const auto* cartridgeCue = Mix_GetChunk(0);
      require(sounds.playNavigationTick(2000) && Mix_GetChunk(0) == cartridgeCue && Mix_PlayingMusic(),
              "Navigation leaves cartridge sounds and background music uninterrupted");
      require(sounds.playMenuOpen() && Mix_GetChunk(0) == cartridgeCue && Mix_PlayingMusic(),
              "Opening a menu plays its own cue without interrupting other audio");
      const auto* openCue = Mix_GetChunk(2);
      require(sounds.playMenuClose() && Mix_GetChunk(2) != openCue && Mix_PlayingMusic(),
              "Closing a menu uses a distinct sound");
      const auto* closeCue = Mix_GetChunk(2);
      require(sounds.playSketchOpen() && Mix_GetChunk(2) != openCue &&
              Mix_GetChunk(2) != closeCue && Mix_PlayingMusic() && !Mix_PausedMusic(),
              "Opening a sketch uses its own loaded cue and leaves music playing");
      const auto* sketchOpenCue = Mix_GetChunk(2);
      require(sounds.playSketchSave() && Mix_GetChunk(0) != cartridgeCue &&
              Mix_GetChunk(2) == sketchOpenCue && Mix_PlayingMusic() && !Mix_PausedMusic(),
              "Save confirmation loads its own sound and leaves menu cues and music alone");
      const auto* saveCue = Mix_GetChunk(0);
      require(sounds.playSketchOpen() && Mix_GetChunk(0) == saveCue,
              "Opening after a save does not replace the save confirmation");
      const int musicGain = Mix_VolumeMusic(-1);
      sounds.setSoundEffectsVolume(40);
      for (int channel = 0; channel < 3; ++channel)
        require(Mix_Volume(channel, -1) == 40 * MIX_MAX_VOLUME / 100,
                "Effects mix applies to cartridge, navigation, and menu channels");
      require(Mix_VolumeChunk(Mix_GetChunk(1), -1) == MIX_MAX_VOLUME / 2 &&
              Mix_VolumeMusic(-1) == musicGain && Mix_PlayingMusic() && !Mix_PausedMusic(),
              "Effects mix preserves quieter ticks and leaves music gain and playback alone");
      sounds.setMusicVolume(60);
      require(Mix_VolumeMusic(-1) == 60 * MIX_MAX_VOLUME / 100 &&
              Mix_Volume(0, -1) == 40 * MIX_MAX_VOLUME / 100,
              "Music mix changes independently of effects");
      sounds.setSoundEffectsVolume(0);
      require(sounds.playMenuOpen() && Mix_Volume(2, -1) == 0 && Mix_PlayingMusic(),
              "Zero effects mix mutes cues without stopping music");
      sounds.setSoundEffectsVolume(255);
      require(Mix_Volume(0, -1) == MIX_MAX_VOLUME, "Effects gain clamps to 100 percent");
      sounds.setSoundEffectsEnabled(false);
      require(!Mix_Playing(0) && !Mix_Playing(1) && !Mix_Playing(2) &&
              !sounds.playCartridgeSlide() && !sounds.playCartridgeInsert() &&
              !sounds.playMenuOpen() && !sounds.playMenuClose() && !sounds.playNavigationTick(2200),
              "Disabling effects stops active cues and silences every effect");
      require(!sounds.playSketchOpen(), "Opening a sketch respects the effects toggle");
      require(!sounds.playSketchSave(), "Saving a sketch respects the effects toggle");
      require(Mix_PlayingMusic() && !Mix_PausedMusic(), "Disabling effects leaves music playing");
      sounds.setSoundEffectsEnabled(true);
      require(sounds.playMenuOpen(), "Re-enabling effects restores cues");
      for (int index = 1; index < static_cast<int>(sounds.trackCount()); ++index)
        require(sounds.nextTrack() && sounds.trackIndex() == index, "Every supplied MP3 decodes in playlist order");
      require(sounds.nextTrack() && sounds.trackIndex() == 0, "Next wraps to the first track");
      require(sounds.nextTrack() && sounds.trackIndex() == 1 && sounds.trackName() == "BGM02",
              "Next advances one track and updates its BGM label");
      require(sounds.previousTrack() && sounds.trackIndex() == 0, "Previous returns to the prior track");
      require(sounds.previousTrack() && sounds.trackIndex() == static_cast<int>(sounds.trackCount()) - 1,
              "Previous wraps from the first track to the last");
      Mix_HaltMusic();
      sounds.update();
      require(sounds.trackIndex() == 0 && sounds.musicEnabled(), "Finishing the last track repeats the playlist");
      sounds.setMusicEnabled(false);
      require(sounds.nextTrack() && !sounds.musicEnabled() && Mix_PausedMusic(),
              "Skipping while paused preserves the pause state");
      require(sounds.open(), "Opening twice keeps the same output");
      require(sounds.playCartridgeInsert(), "The cartridge sound queues for nonblocking playback");
      require(sounds.playCartridgeInsert(), "A new insertion can restart the effect");
      sounds.setSoundEffectsVolume(40);
      sounds.close();
      sounds.close();
      require(!sounds.playCartridgeSlide(), "Slide playback after close is harmless");
      require(!sounds.playNavigationTick(2200), "Navigation playback after close is harmless");
      require(!sounds.playCartridgeInsert(), "Playback after close is harmless");
      require(!sounds.playSketchOpen(), "Sketch playback after close is harmless");
      require(!sounds.playSketchSave(), "Save playback after close is harmless");
      require((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
              "Closing releases the audio subsystem reference");
      require(sounds.open() && sounds.playCartridgeInsert(), "Audio can reopen after close");
      require(Mix_Volume(0, -1) == 40 * MIX_MAX_VOLUME / 100,
              "Effects gain is reapplied when audio reopens");
    }
  }
  if (!unavailable) {
    const auto directory = std::filesystem::temp_directory_path() /
        ("bitmap16-invalid-music-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directory(directory);
    std::ofstream(directory / "00-invalid.mp3") << "not an MP3";
    bitmap16::Desktop::AudioPlayer invalid;
    require(invalid.open(directory) && !invalid.setMusicEnabled(true) && !invalid.musicEnabled(),
            "An unreadable playlist disables music without affecting the app");
    require(invalid.playCartridgeInsert(), "Sound effects survive an unreadable playlist");
    invalid.close();
    const auto source = std::filesystem::directory_iterator(BITMAP16_MUSIC_TEST_DIR)->path();
    std::filesystem::copy_file(source, directory / "01-valid.mp3");
    require(invalid.open(directory) && invalid.setMusicEnabled(true) && invalid.trackIndex() == 1,
            "An invalid track is skipped in favor of the next playable MP3");
    invalid.close();
    std::filesystem::remove_all(directory);
  }
  require((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0,
          "Destruction releases audio before SDL shutdown");
  SDL_Quit();
}
