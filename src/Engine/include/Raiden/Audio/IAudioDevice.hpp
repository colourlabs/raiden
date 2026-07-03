#pragma once

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/EngineConfig.hpp>

#include <cstdint>
#include <string_view>

namespace Raiden::Audio {

class IAudioDevice {
public:
  virtual ~IAudioDevice() = default;

  virtual bool init(const ::Raiden::Core::AudioConfig &config, ::Raiden::Core::IVirtualFileSystem &vfs) = 0;
  virtual void shutdown() = 0;

  using SoundId = uint32_t;
  using VoiceId = uint32_t;

  // load a sound file (WAV, FLAC, Ogg Vorbis, Opus)
  virtual SoundId load(std::string_view path) = 0;
  virtual void unload(SoundId sound) = 0;

  // play a loaded sound
  virtual VoiceId play(SoundId sound, float volume = 1.0f, float pitch = 1.0f,
                       bool loop = false) = 0;
  virtual void stop(VoiceId voice) = 0;
  virtual void setVolume(VoiceId voice, float volume) = 0;
  virtual void setPitch(VoiceId voice, float pitch) = 0;
  virtual bool isPlaying(VoiceId voice) const = 0;
  virtual float getPosition(VoiceId voice) const = 0;

  // 3D positional audio
  virtual void setListenerPosition(float x, float y, float z) = 0;
  virtual void setListenerOrientation(float atX, float atY, float atZ,
                                      float upX, float upY, float upZ) = 0;
  virtual void setPosition(VoiceId voice, float x, float y, float z) = 0;
  virtual void setVelocity(VoiceId voice, float x, float y, float z) = 0;

  virtual void setMasterVolume(float vol) = 0;
  virtual float masterVolume() const = 0;

  // called once per frame from the main thread to finalize
  // background-decoded audio (uploads OpenAL buffers)
  virtual void processPendingLoads() = 0;
};

} // namespace Raiden::Audio
