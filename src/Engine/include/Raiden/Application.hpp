#pragma once

#include <Raiden/Audio/IAudioDevice.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Engine/GamePluginLoader.hpp>
#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/EngineConfig.hpp>
#include <Raiden/Jobs/JobSystem.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>

#include <chrono>
#include <memory>

namespace Raiden::Engine {

class Application {
public:
  Application(std::unique_ptr<::Raiden::Platform::IPlatform> platform,
              std::unique_ptr<::Raiden::Renderer::IRenderDevice> device,
              std::unique_ptr<::Raiden::Core::IVirtualFileSystem> vfs);
  ~Application();

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;
  Application(Application &&) = delete;
  Application &operator=(Application &&) = delete;

  bool init(const ::Raiden::Core::EngineConfig &config);
  void shutdown();
  void run();

  bool loadGamePlugin(std::string_view path);

  [[nodiscard]] ::Raiden::Platform::IPlatform &getPlatform() const { return *platform_; }
  [[nodiscard]] ::Raiden::Renderer::IRenderDevice &getDevice() const { return *device_; }
  [[nodiscard]] ::Raiden::Core::IVirtualFileSystem &getVFS() const { return *vfs_; }
  [[nodiscard]] ::Raiden::Jobs::JobSystem &getJobSystem() { return jobSystem_; }

private:
  std::unique_ptr<::Raiden::Platform::IPlatform> platform_;
  std::unique_ptr<::Raiden::Renderer::IRenderDevice> device_;
  std::unique_ptr<::Raiden::Core::IVirtualFileSystem> vfs_;
  std::unique_ptr<::Raiden::Assets::IAssetManager> assetManager_;
  std::unique_ptr<::Raiden::Audio::IAudioDevice> audioDevice_;
  std::unique_ptr<ImGuiOverlay> overlay_;

  ::Raiden::Jobs::JobSystem jobSystem_;
  ::Raiden::Core::EngineConfig config_;
  ::Raiden::Engine::GamePluginLoader pluginLoader_;

  bool running_ = false;
  std::chrono::steady_clock::time_point lastFrameTime_;
};

} // namespace Raiden::Engine
