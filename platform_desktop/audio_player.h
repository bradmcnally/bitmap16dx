#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

#include <filesystem>
#include <array>
#include <random>
#include <string>
#include <vector>

namespace bitmap16::Desktop {

// One mixer owns app-wide music and sound effects; views never own playback.
class AudioPlayer {
 public:
  AudioPlayer() = default;
  ~AudioPlayer();
  AudioPlayer(const AudioPlayer&) = delete;
  AudioPlayer& operator=(const AudioPlayer&) = delete;

  bool open(const std::filesystem::path& musicDirectory = {});
  bool playCartridgeSlide();
  bool playCartridgeInsert();
  bool playMenuOpen();
  bool playMenuClose();
  bool playSketchOpen();
  bool playSketchSave();
  bool playNavigationTick(Uint32 nowMs = SDL_GetTicks());
  void setSoundEffectsEnabled(bool enabled);
  void setSoundEffectsVolume(uint8_t percent);
  bool setMusicEnabled(bool enabled);
  void setMusicVolume(uint8_t percent);
  bool previousTrack();
  bool nextTrack();
  void update();
  void close();
  bool musicEnabled() const { return musicEnabled_; }
  std::size_t trackCount() const { return tracks_.size(); }
  int trackIndex() const { return trackIndex_; }
  std::string trackName() const;

 private:
  bool selectTrack(int index, int direction = 1);
  bool initialized_ = false;
  bool opened_ = false;
  bool mp3Ready_ = false;
  bool musicEnabled_ = false;
  bool soundEffectsEnabled_ = true;
  uint8_t soundEffectsVolume_ = 100;
  Mix_Chunk* cartridgeSlide_ = nullptr;
  Mix_Chunk* cartridgeInsert_ = nullptr;
  Mix_Chunk* menuOpen_ = nullptr;
  Mix_Chunk* menuClose_ = nullptr;
  Mix_Chunk* sketchOpen_ = nullptr;
  Mix_Chunk* sketchSave_ = nullptr;
  std::array<Mix_Chunk*, 5> navigationTicks_{};
  std::mt19937 navigationRandom_;
  std::size_t lastNavigationSound_ = 5;
  Uint32 lastNavigationAt_ = 0;
  bool navigationPlayed_ = false;
  Mix_Music* music_ = nullptr;
  std::vector<std::filesystem::path> tracks_;
  int trackIndex_ = -1;
};

}  // namespace bitmap16::Desktop
