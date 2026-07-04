#include <Raiden/Audio/OpenALDevice.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>

#define MINIAUDIO_IMPLEMENTATION
#include <math.h>
#include <miniaudio.h>

#include <AL/al.h>
#include <AL/alc.h>

#ifdef RAIDEN_HAS_OPUS
#include <opusfile.h>
#endif

namespace Raiden::Audio {

using namespace ::Raiden::Core;
using namespace ::Raiden::Jobs;

OpenALDevice::OpenALDevice() = default;

OpenALDevice::~OpenALDevice() { shutdown(); }

bool OpenALDevice::init(const AudioConfig &config, IVirtualFileSystem &vfs) {
  vfs_ = &vfs;
  masterVolume_ = config.masterVolume;
  alcDevice_ = alcOpenDevice(nullptr);

  if (alcDevice_ == nullptr) {
    return false;
  }

  alcContext_ = alcCreateContext(static_cast<ALCdevice *>(alcDevice_), nullptr);

  if (alcContext_ == nullptr) {
    alcCloseDevice(static_cast<ALCdevice *>(alcDevice_));
    alcDevice_ = nullptr;
    return false;
  }

  alcMakeContextCurrent(static_cast<ALCcontext *>(alcContext_));
  alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

  initialized_ = true;
  return true;
}

void OpenALDevice::shutdown() {
  for (auto &[id, voice] : voices_) {
    alSourceStop(voice.source);
    alDeleteSources(1, &voice.source);
  }
  voices_.clear();

  for (auto &[id, sound] : sounds_) {
    alDeleteBuffers(1, &sound.buffer);
  }
  sounds_.clear();

  {
    std::lock_guard<std::mutex> lock(pendingMutex_);
    pendingLoads_.clear();
  }

  if (alcContext_ != nullptr) {
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(static_cast<ALCcontext *>(alcContext_));
    alcContext_ = nullptr;
  }

  if (alcDevice_ != nullptr) {
    alcCloseDevice(static_cast<ALCdevice *>(alcDevice_));
    alcDevice_ = nullptr;
  }

  initialized_ = false;
}

OpenALDevice::SoundId OpenALDevice::load(std::string_view path) {
  if (!initialized_) {
    return 0;
  }

  if (jobSystem_ != nullptr) {
    // async path: read on main thread, decode on worker, upload later
    auto fileData = vfs_->readBytes(path);
    if (fileData.empty()) {
      return 0;
    }

    auto pending = std::make_unique<PendingDecode>();

    SoundId id = nextSoundId_++;

    JobDesc desc;
    desc.task = [pending = pending.get(),
                 data = std::move(fileData)]() mutable {
      // decode on worker thread
      ma_decoder decoder;
      ma_decoder_config config =
          ma_decoder_config_init(ma_format_s16, 2, 48000);

      if (ma_decoder_init_memory(data.data(), data.size(), &config, &decoder) ==
          MA_SUCCESS) {
        int channels = static_cast<int>(decoder.outputChannels);
        int sampleRate = static_cast<int>(decoder.outputSampleRate);

        ma_uint64 cap = 65536;
        pending->samples.resize(cap * channels);

        ma_uint64 total = 0;
        for (;;) {
          ma_uint64 framesRead = 0;
          ma_result result = ma_decoder_read_pcm_frames(
              &decoder, pending->samples.data() + (total * channels),
              cap - total, &framesRead);

          total += framesRead;

          if (framesRead == 0 || result != MA_SUCCESS) {
            break;
          }

          if (total == cap) {
            cap *= 2;
            pending->samples.resize(cap * channels);
          }
        }

        pending->samples.resize(total * channels);
        pending->channels = channels;
        pending->sampleRate = sampleRate;
        ma_decoder_uninit(&decoder);
        pending->decodeFailed = (total == 0);
      } else {
        pending->decodeFailed = true;
      }

      pending->ready.store(true, std::memory_order_release);
    };

    {
      std::lock_guard<std::mutex> lock(pendingMutex_);
      pendingLoads_[id] = std::move(pending);
    }

    jobSystem_->submit(std::move(desc));
    return id;
  }

  // synchronous fallback when no job system is set
  std::vector<int16_t> samples;
  int channels = 0, sampleRate = 0;
  if (!decodeFile(path, samples, channels, sampleRate)) {
    return 0;
  }

  ALenum format = 0;

  if (channels == 1) {
    format = AL_FORMAT_MONO16;
  } else if (channels == 2) {
    format = AL_FORMAT_STEREO16;
  } else {
    return 0;
  }

  ALuint buffer = 0;
  alGenBuffers(1, &buffer);
  alBufferData(buffer, format, samples.data(),
               static_cast<ALsizei>(samples.size() * sizeof(int16_t)),
               sampleRate);

  if (alGetError() != AL_NO_ERROR) {
    alDeleteBuffers(1, &buffer);
    return 0;
  }

  SoundId id = nextSoundId_++;
  sounds_[id] = {.buffer = buffer,
                 .duration = static_cast<float>(samples.size()) /
                             static_cast<float>(channels * sampleRate)};
  return id;
}

void OpenALDevice::processPendingLoads() {
  std::lock_guard<std::mutex> lock(pendingMutex_);

  auto it = pendingLoads_.begin();
  while (it != pendingLoads_.end()) {
    auto &pending = it->second;
    if (!pending->ready.load(std::memory_order_acquire)) {
      ++it;
      continue;
    }

    SoundId id = it->first;

    if (pending->decodeFailed || pending->samples.empty()) {
      pendingLoads_.erase(it++);
      continue;
    }

    ALenum format = 0;
    if (pending->channels == 1) {
      format = AL_FORMAT_MONO16;
    } else if (pending->channels == 2) {
      format = AL_FORMAT_STEREO16;
    } else {
      pendingLoads_.erase(it++);
      continue;
    }

    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(
        buffer, format, pending->samples.data(),
        static_cast<ALsizei>(pending->samples.size() * sizeof(int16_t)),
        pending->sampleRate);

    if (alGetError() == AL_NO_ERROR) {
      sounds_[id] = {.buffer = buffer,
                     .duration = static_cast<float>(pending->samples.size()) /
                                 static_cast<float>(pending->channels *
                                                    pending->sampleRate)};
    }

    pendingLoads_.erase(it++);
  }
}

void OpenALDevice::unload(SoundId sound) {
  // check loaded sounds
  auto it = sounds_.find(sound);
  if (it != sounds_.end()) {
    for (auto &[vid, voice] : voices_) {
      if (voice.soundId == sound) {
        stop(vid);
      }
    }
    alDeleteBuffers(1, &it->second.buffer);
    sounds_.erase(it);
  }

  // check pending loads
  std::lock_guard<std::mutex> lock(pendingMutex_);
  auto pit = pendingLoads_.find(sound);
  if (pit != pendingLoads_.end()) {
    pendingLoads_.erase(pit);
  }
}

OpenALDevice::VoiceId OpenALDevice::play(SoundId sound, float volume,
                                         float pitch, bool loop) {
  if (!initialized_) {
    return 0;
  }

  auto it = sounds_.find(sound);
  if (it == sounds_.end()) {
    return 0;
  }

  ALuint source = 0;
  alGenSources(1, &source);
  alSourcei(source, AL_BUFFER, static_cast<ALint>(it->second.buffer));
  alSourcef(source, AL_GAIN, volume);
  alSourcef(source, AL_PITCH, pitch);
  alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
  alSourcePlay(source);

  if (alGetError() != AL_NO_ERROR) {
    alDeleteSources(1, &source);
    return 0;
  }

  VoiceId id = nextVoiceId_++;
  voices_[id] = {.source = source, .soundId = sound};
  return id;
}

void OpenALDevice::stop(VoiceId voice) {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return;
  }

  alSourceStop(it->second.source);
  alDeleteSources(1, &it->second.source);
  voices_.erase(it);
}

void OpenALDevice::setVolume(VoiceId voice, float volume) {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return;
  }
  alSourcef(it->second.source, AL_GAIN, volume);
}

void OpenALDevice::setPitch(VoiceId voice, float pitch) {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return;
  }
  alSourcef(it->second.source, AL_PITCH, pitch);
}

bool OpenALDevice::isPlaying(VoiceId voice) const {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return false;
  }

  ALint state = 0;
  alGetSourcei(it->second.source, AL_SOURCE_STATE, &state);
  return state == AL_PLAYING;
}

float OpenALDevice::getPosition(VoiceId voice) const {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return 0.0F;
  }

  ALfloat offset = 0.0F;
  alGetSourcef(it->second.source, AL_SEC_OFFSET, &offset);
  return offset;
}

void OpenALDevice::setListenerPosition(float x, float y, float z) {
  alListener3f(AL_POSITION, x, y, z);
}

void OpenALDevice::setListenerOrientation(float atX, float atY, float atZ,
                                          float upX, float upY, float upZ) {
  std::array<ALfloat, 6> orient = {atX, atY, atZ, upX, upY, upZ};
  alListenerfv(AL_ORIENTATION, orient.data());
}

void OpenALDevice::setPosition(VoiceId voice, float x, float y, float z) {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return;
  }
  alSource3f(it->second.source, AL_POSITION, x, y, z);
}

void OpenALDevice::setVelocity(VoiceId voice, float x, float y, float z) {
  auto it = voices_.find(voice);
  if (it == voices_.end()) {
    return;
  }
  alSource3f(it->second.source, AL_VELOCITY, x, y, z);
}

void OpenALDevice::setMasterVolume(float vol) {
  masterVolume_ = vol;
  for (auto &[id, voice] : voices_) {
    ALfloat gain = NAN;
    alGetSourcef(voice.source, AL_GAIN, &gain);
    alSourcef(voice.source, AL_GAIN, gain);
  }
}

float OpenALDevice::masterVolume() const { return masterVolume_; }

bool OpenALDevice::decodeFile(std::string_view path,
                              std::vector<int16_t> &samples, int &channels,
                              int &sampleRate) {
  if (vfs_ == nullptr) {
    return false;
  }

  auto data = vfs_->readBytes(path);
  if (data.empty()) {
    return false;
  }

  // Try miniaudio (WAV, FLAC, MP3, Vorbis)
  ma_decoder decoder;
  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 2, 48000);

  if (ma_decoder_init_memory(data.data(), data.size(), &config, &decoder) ==
      MA_SUCCESS) {
    channels = static_cast<int>(decoder.outputChannels);
    sampleRate = static_cast<int>(decoder.outputSampleRate);

    ma_uint64 cap = 65536;
    samples.resize(cap * channels);

    ma_uint64 total = 0;
    for (;;) {
      ma_uint64 framesRead = 0;
      ma_result result = ma_decoder_read_pcm_frames(
          &decoder, samples.data() + (total * channels), cap - total,
          &framesRead);

      total += framesRead;

      if (framesRead == 0 || result != MA_SUCCESS) {
        break;
      }

      if (total == cap) {
        cap *= 2;
        samples.resize(cap * channels);
      }
    }

    samples.resize(total * channels);
    ma_decoder_uninit(&decoder);
    return total > 0;
  }

#ifdef RAIDEN_HAS_OPUS
  int err = 0;
  OggOpusFile *of =
      op_open_memory(reinterpret_cast<const unsigned char *>(data.data()),
                     static_cast<opus_int32>(data.size()), &err);
  if (of) {
    int opusChannels = op_channel_count(of, -1);
    if (opusChannels < 1 || opusChannels > 2) {
      op_free(of);
      return false;
    }

    int bufSize = 4096 * opusChannels;
    auto *pcm = new opus_int16[static_cast<size_t>(bufSize)];
    samples.reserve(1048576);

    int samplesPerChannel;
    while ((samplesPerChannel =
                op_read_stereo(of, pcm, bufSize / opusChannels)) > 0) {
      samples.insert(samples.end(), pcm,
                     pcm + samplesPerChannel * opusChannels);
    }

    delete[] pcm;
    op_free(of);

    channels = opusChannels;
    sampleRate = 48000;
    return !samples.empty();
  }
#endif

  return false;
}

} // namespace Raiden::Audio
