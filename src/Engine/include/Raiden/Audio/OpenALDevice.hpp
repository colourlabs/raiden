#pragma once

#include <Raiden/Audio/IAudioDevice.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Jobs/JobSystem.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

using ALuint = unsigned int;

namespace Raiden::Audio {

class OpenALDevice : public IAudioDevice {
public:
  OpenALDevice();
  ~OpenALDevice() override;

  OpenALDevice(const OpenALDevice &) = delete;
  OpenALDevice &operator=(const OpenALDevice &) = delete;
  OpenALDevice(OpenALDevice &&) = delete;
  OpenALDevice &operator=(OpenALDevice &&) = delete;

  bool init(const ::Raiden::Core::AudioConfig &config, ::Raiden::Core::IVirtualFileSystem &vfs) override;
  void shutdown() override;

  void setJobSystem(::Raiden::Jobs::JobSystem &js) { jobSystem_ = &js; }

  SoundId load(std::string_view path) override;
  void unload(SoundId sound) override;

  VoiceId play(SoundId sound, float volume = 1.0F, float pitch = 1.0F,
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

  void processPendingLoads() override;

private:
  struct LoadedSound {
    ALuint buffer = 0;
    float duration = 0.0F;
  };

  struct ActiveVoice {
    ALuint source = 0;
    SoundId soundId = 0;
  };

  struct PendingDecode {
    std::vector<int16_t> samples;
    int channels = 0;
    int sampleRate = 0;
    std::atomic<bool> ready{false};
    bool decodeFailed = false;
  };

  bool decodeFile(std::string_view path, std::vector<int16_t> &samples,
                  int &channels, int &sampleRate);

  ::Raiden::Core::IVirtualFileSystem *vfs_ = nullptr;
  ::Raiden::Jobs::JobSystem *jobSystem_ = nullptr;

  void *alcDevice_ = nullptr;
  void *alcContext_ = nullptr;

  std::unordered_map<SoundId, LoadedSound> sounds_;
  std::unordered_map<VoiceId, ActiveVoice> voices_;

  std::mutex pendingMutex_;
  std::unordered_map<SoundId, std::unique_ptr<PendingDecode>> pendingLoads_;

  SoundId nextSoundId_ = 1;
  VoiceId nextVoiceId_ = 1;
  float masterVolume_ = 1.0F;
  bool initialized_ = false;
};

} // namespace Raiden::Audio
