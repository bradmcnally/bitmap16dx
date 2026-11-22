#include "audio_player.h"
#include "cartridge_sound.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace bitmap16::Desktop {

AudioPlayer::~AudioPlayer() { close(); }

bool AudioPlayer::open(const std::filesystem::path& musicDirectory) {
  if (opened_) return true;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    SDL_Log("Audio unavailable: %s", SDL_GetError());
    return false;
  }
  initialized_ = true;
  mp3Ready_ = (Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) != 0;
  if (!mp3Ready_) SDL_Log("MP3 decoder unavailable: %s", Mix_GetError());
  if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512) != 0) {
    SDL_Log("Audio output unavailable: %s", Mix_GetError());
    close();
    return false;
  }
  opened_ = true;
  cartridgeSlide_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kCartridgeSlide,
      static_cast<int>(sizeof(AudioAssets::kCartridgeSlide))), 1);
  if (cartridgeSlide_ == nullptr)
    SDL_Log("Cartridge slide sound could not be loaded: %s", Mix_GetError());
  cartridgeInsert_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kCartridgeInsert,
      static_cast<int>(sizeof(AudioAssets::kCartridgeInsert))), 1);
  if (cartridgeInsert_ == nullptr)
    SDL_Log("Cartridge sound could not be loaded: %s", Mix_GetError());
  menuOpen_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kMenuOpen,
      static_cast<int>(sizeof(AudioAssets::kMenuOpen))), 1);
  menuClose_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kMenuClose,
      static_cast<int>(sizeof(AudioAssets::kMenuClose))), 1);
  if (menuOpen_ == nullptr || menuClose_ == nullptr)
    SDL_Log("Menu sounds could not be loaded: %s", Mix_GetError());
  sketchOpen_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kSketchOpen,
      static_cast<int>(sizeof(AudioAssets::kSketchOpen))), 1);
  if (sketchOpen_ == nullptr)
    SDL_Log("Sketch open sound could not be loaded: %s", Mix_GetError());
  sketchSave_ = Mix_LoadWAV_RW(SDL_RWFromConstMem(AudioAssets::kSketchSave,
      static_cast<int>(sizeof(AudioAssets::kSketchSave))), 1);
  if (sketchSave_ == nullptr)
    SDL_Log("Sketch save sound could not be loaded: %s", Mix_GetError());
  navigationRandom_.seed(static_cast<Uint32>(SDL_GetPerformanceCounter()));
  for (std::size_t i = 0; i < navigationTicks_.size(); ++i) {
    const auto& sound = AudioAssets::kNavigationSounds[i];
    navigationTicks_[i] = Mix_LoadWAV_RW(SDL_RWFromConstMem(sound.data, sound.size), 1);
    if (navigationTicks_[i] != nullptr) Mix_VolumeChunk(navigationTicks_[i], MIX_MAX_VOLUME / 2);
    else SDL_Log("Navigation sound %zu could not be loaded: %s", i + 1, Mix_GetError());
  }

  std::error_code error;
  if (!musicDirectory.empty()) {
    std::filesystem::directory_iterator entry(musicDirectory, error);
    for (const std::filesystem::directory_iterator end; !error && entry != end;
         entry.increment(error)) {
      std::error_code fileError;
      if (!entry->is_regular_file(fileError)) continue;
      std::string extension = entry->path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (extension == ".mp3") tracks_.push_back(entry->path());
    }
    std::sort(tracks_.begin(), tracks_.end());
  }
  setSoundEffectsVolume(soundEffectsVolume_);
  return true;
}

bool AudioPlayer::playCartridgeSlide() {
  return soundEffectsEnabled_ && opened_ && cartridgeSlide_ != nullptr &&
         Mix_PlayChannel(0, cartridgeSlide_, 0) >= 0;
}

bool AudioPlayer::playCartridgeInsert() {
  return soundEffectsEnabled_ && opened_ && cartridgeInsert_ != nullptr &&
         Mix_PlayChannel(0, cartridgeInsert_, 0) >= 0;
}

bool AudioPlayer::playMenuOpen() {
  return soundEffectsEnabled_ && opened_ && menuOpen_ != nullptr && Mix_PlayChannel(2, menuOpen_, 0) >= 0;
}

bool AudioPlayer::playMenuClose() {
  return soundEffectsEnabled_ && opened_ && menuClose_ != nullptr && Mix_PlayChannel(2, menuClose_, 0) >= 0;
}

bool AudioPlayer::playSketchOpen() {
  return soundEffectsEnabled_ && opened_ && sketchOpen_ != nullptr && Mix_PlayChannel(2, sketchOpen_, 0) >= 0;
}

bool AudioPlayer::playSketchSave() {
  // Keep save confirmation separate from menu cues during Save and Open.
  return soundEffectsEnabled_ && opened_ && sketchSave_ != nullptr && Mix_PlayChannel(0, sketchSave_, 0) >= 0;
}

bool AudioPlayer::playNavigationTick(Uint32 nowMs) {
  if (!soundEffectsEnabled_ || !opened_ || (navigationPlayed_ && nowMs - lastNavigationAt_ < 100u)) return false;
  std::array<std::size_t, 5> choices{};
  std::size_t count = 0;
  for (std::size_t i = 0; i < navigationTicks_.size(); ++i)
    if (navigationTicks_[i] != nullptr && i != lastNavigationSound_) choices[count++] = i;
  if (count == 0) {
    for (std::size_t i = 0; i < navigationTicks_.size(); ++i)
      if (navigationTicks_[i] != nullptr) { choices[count++] = i; break; }
  }
  if (count == 0) return false;
  const auto selected = choices[std::uniform_int_distribution<std::size_t>(0, count - 1)(navigationRandom_)];
  // A separate channel keeps navigation from interrupting cartridge effects.
  if (Mix_PlayChannel(1, navigationTicks_[selected], 0) < 0) return false;
  lastNavigationSound_ = selected;
  lastNavigationAt_ = nowMs;
  navigationPlayed_ = true;
  return true;
}

void AudioPlayer::setSoundEffectsEnabled(bool enabled) {
  soundEffectsEnabled_ = enabled;
  if (opened_ && !enabled) {
    Mix_HaltChannel(0);
    Mix_HaltChannel(1);
    Mix_HaltChannel(2);
  }
}

void AudioPlayer::setSoundEffectsVolume(uint8_t percent) {
  soundEffectsVolume_ = std::min<unsigned>(percent, 100);
  if (!opened_) return;
  // Channel gain preserves each sample's balance, including quieter ticks.
  const int gain = soundEffectsVolume_ * MIX_MAX_VOLUME / 100;
  for (int channel = 0; channel < 3; ++channel) Mix_Volume(channel, gain);
}

void AudioPlayer::setMusicVolume(uint8_t percent) {
  if (opened_) Mix_VolumeMusic(std::min<unsigned>(percent, 100) * MIX_MAX_VOLUME / 100);
}

bool AudioPlayer::selectTrack(int index, int direction) {
  if (!opened_ || !mp3Ready_ || tracks_.empty()) return false;
  Mix_HaltMusic();
  if (music_ != nullptr) { Mix_FreeMusic(music_); music_ = nullptr; }
  const int count = static_cast<int>(tracks_.size());
  for (int attempt = 0; attempt < count; ++attempt) {
    index = (index % count + count) % count;
    music_ = Mix_LoadMUS(tracks_[index].string().c_str());
    if (music_ != nullptr && Mix_PlayMusic(music_, 0) == 0) {
      trackIndex_ = index;
      if (!musicEnabled_) Mix_PauseMusic();
      return true;
    }
    SDL_Log("Music track unavailable (%s): %s", tracks_[index].filename().string().c_str(), Mix_GetError());
    if (music_ != nullptr) { Mix_FreeMusic(music_); music_ = nullptr; }
    index += direction;
  }
  musicEnabled_ = false;
  return false;
}

bool AudioPlayer::setMusicEnabled(bool enabled) {
  if (!opened_ || !mp3Ready_ || tracks_.empty()) {
    musicEnabled_ = false;
    return !enabled;
  }
  musicEnabled_ = enabled;
  if (!enabled) { Mix_PauseMusic(); return true; }
  if (music_ != nullptr && Mix_PlayingMusic()) { Mix_ResumeMusic(); return true; }
  return selectTrack(trackIndex_ < 0 ? 0 : trackIndex_);
}

bool AudioPlayer::previousTrack() {
  return selectTrack(trackIndex_ < 0 ? static_cast<int>(tracks_.size()) - 1 : trackIndex_ - 1, -1);
}

bool AudioPlayer::nextTrack() { return selectTrack(trackIndex_ < 0 ? 0 : trackIndex_ + 1); }

void AudioPlayer::update() {
  if (opened_ && musicEnabled_ && !Mix_PlayingMusic()) nextTrack();
}

std::string AudioPlayer::trackName() const {
  if (trackIndex_ < 0) return {};
  char name[32];
  std::snprintf(name, sizeof(name), "BGM%02u", static_cast<unsigned>(trackIndex_) + 1);
  return name;
}

void AudioPlayer::close() {
  if (opened_) {
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    if (music_ != nullptr) Mix_FreeMusic(music_);
    if (cartridgeSlide_ != nullptr) Mix_FreeChunk(cartridgeSlide_);
    if (cartridgeInsert_ != nullptr) Mix_FreeChunk(cartridgeInsert_);
    if (menuOpen_ != nullptr) Mix_FreeChunk(menuOpen_);
    if (menuClose_ != nullptr) Mix_FreeChunk(menuClose_);
    if (sketchOpen_ != nullptr) Mix_FreeChunk(sketchOpen_);
    if (sketchSave_ != nullptr) Mix_FreeChunk(sketchSave_);
    for (auto* sound : navigationTicks_) if (sound != nullptr) Mix_FreeChunk(sound);
    Mix_CloseAudio();
  }
  music_ = nullptr;
  cartridgeSlide_ = nullptr;
  cartridgeInsert_ = nullptr;
  menuOpen_ = menuClose_ = nullptr;
  sketchOpen_ = nullptr;
  sketchSave_ = nullptr;
  navigationTicks_.fill(nullptr);
  navigationPlayed_ = false;
  lastNavigationSound_ = navigationTicks_.size();
  opened_ = mp3Ready_ = musicEnabled_ = false;
  tracks_.clear();
  trackIndex_ = -1;
  if (initialized_) {
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_ = false;
  }
}

}  // namespace bitmap16::Desktop
