#pragma once

#include <RaidenEngineCore/Audio/IAudioDevice.hpp>

#include <unordered_map>
#include <vector>

typedef unsigned int ALuint;

namespace Raiden::Core {

class IVirtualFileSystem;

class OpenALDevice : public IAudioDevice {
public:
  OpenALDevice();
  ~OpenALDevice() override;

  OpenALDevice(const OpenALDevice &) = delete;
  OpenALDevice &operator=(const OpenALDevice &) = delete;
  OpenALDevice(OpenALDevice &&) = delete;
  OpenALDevice &operator=(OpenALDevice &&) = delete;

  bool init(const AudioConfig &config, IVirtualFileSystem &vfs) override;
  void shutdown() override;

  SoundId load(std::string_view path) override;
  void unload(SoundId sound) override;

  VoiceId play(SoundId sound, float volume = 1.0f, float pitch = 1.0f,
               bool loop = false) override;
  void stop(VoiceId voice) override;
  void setVolume(VoiceId voice, float volume) override;
  void setPitch(VoiceId voice, float pitch) override;
  bool isPlaying(VoiceId voice) const override;
  float getPosition(VoiceId voice) const override;

  void setListenerPosition(float x, float y, float z) override;
  void setListenerOrientation(float atX, float atY, float atZ, float upX,
                              float upY, float upZ) override;
  void setPosition(VoiceId voice, float x, float y, float z) override;
  void setVelocity(VoiceId voice, float x, float y, float z) override;

  void setMasterVolume(float vol) override;
  float masterVolume() const override;

private:
  struct LoadedSound {
    ALuint buffer = 0;
    float duration = 0.0f;
  };

  struct ActiveVoice {
    ALuint source = 0;
    SoundId soundId = 0;
  };

  bool decodeFile(std::string_view path, std::vector<int16_t> &samples,
                  int &channels, int &sampleRate);

  IVirtualFileSystem *vfs_ = nullptr;

  void *alcDevice_ = nullptr;
  void *alcContext_ = nullptr;

  std::unordered_map<SoundId, LoadedSound> sounds_;
  std::unordered_map<VoiceId, ActiveVoice> voices_;

  SoundId nextSoundId_ = 1;
  VoiceId nextVoiceId_ = 1;
  float masterVolume_ = 1.0f;
  bool initialized_ = false;
};

} // namespace Raiden::Core
