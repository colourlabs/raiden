#pragma once

#include <RaidenEngineCore/Audio/IAudioDevice.hpp>
#include <RaidenEngineCore/Core/IVirtualFileSystem.hpp>
#include <RaidenEngineCore/Engine/GamePluginLoader.hpp>
#include <RaidenEngineCore/Engine/ImGuiOverlay.hpp>
#include <RaidenEngineCore/EngineConfig.hpp>
#include <RaidenEngineCore/Jobs/JobSystem.hpp>
#include <RaidenEngineCore/Platform/IPlatform.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>

#include <chrono>
#include <memory>

namespace Raiden::Core {

class Application {
public:
  Application(std::unique_ptr<IPlatform> platform,
              std::unique_ptr<IRenderDevice> device,
              std::unique_ptr<IVirtualFileSystem> vfs);
  ~Application();

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;
  Application(Application &&) = delete;
  Application &operator=(Application &&) = delete;

  bool init(const EngineConfig &config);
  void shutdown();
  void run();

  bool loadGamePlugin(std::string_view path);

  [[nodiscard]] IPlatform &getPlatform() const { return *platform_; }
  [[nodiscard]] IRenderDevice &getDevice() const { return *device_; }
  [[nodiscard]] IVirtualFileSystem &getVFS() const { return *vfs_; }
  [[nodiscard]] JobSystem &getJobSystem() { return jobSystem_; }

private:
  std::unique_ptr<IPlatform> platform_;
  std::unique_ptr<IRenderDevice> device_;
  std::unique_ptr<IVirtualFileSystem> vfs_;
  std::unique_ptr<IAssetManager> assetManager_;
  std::unique_ptr<IAudioDevice> audioDevice_;
  std::unique_ptr<ImGuiOverlay> overlay_;

  JobSystem jobSystem_;
  EngineConfig config_;
  GamePluginLoader pluginLoader_;

  bool running_ = false;
  std::chrono::steady_clock::time_point lastFrameTime_;
};

} // namespace Raiden::Core
