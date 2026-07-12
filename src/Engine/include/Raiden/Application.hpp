#pragma once

#include <Raiden/Audio/IAudioDevice.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Engine/GamePluginLoader.hpp>
#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/EngineConfig.hpp>
#include <Raiden/Jobs/JobSystem.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Physics/PhysicsSyncSystem.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>

#include <atomic>
#include <chrono>
#include <functional>
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
  void requestShutdown();

  bool loadGamePlugin(std::string_view path);
  bool registerPlugin(Raiden::Engine::IGamePlugin *plugin);

  [[nodiscard]] ::Raiden::Platform::IPlatform &getPlatform() const {
    return *platform_;
  }
  [[nodiscard]] ::Raiden::Renderer::IRenderDevice &getDevice() const {
    return *device_;
  }
  [[nodiscard]] ::Raiden::Core::IVirtualFileSystem &getVFS() const {
    return *vfs_;
  }
  [[nodiscard]] ::Raiden::Jobs::JobSystem &getJobSystem() { return jobSystem_; }
  [[nodiscard]] ImGuiOverlay *getOverlay() const { return overlay_.get(); }
  [[nodiscard]] ::Raiden::Physics::IPhysicsSystem *getPhysics() const {
    return physicsSystem_.get();
  }

  [[nodiscard]] ::Raiden::ECS::World *getWorld() {
    if (!pluginLoader_.isLoaded()) {
      return nullptr;
    }
    return pluginLoader_.plugin().getWorld();
  }

  using GizmoRenderCallback =
      std::function<void(::Raiden::Renderer::ICommandBuffer &)>;
  void setGizmoRenderCallback(GizmoRenderCallback cb) {
    gizmoRenderCb_ = std::move(cb);
  }

private:
  std::unique_ptr<::Raiden::Platform::IPlatform> platform_;
  std::unique_ptr<::Raiden::Renderer::IRenderDevice> device_;
  std::unique_ptr<::Raiden::Core::IVirtualFileSystem> vfs_;
  std::unique_ptr<::Raiden::Assets::IAssetManager> assetManager_;
  std::unique_ptr<::Raiden::Audio::IAudioDevice> audioDevice_;
  std::unique_ptr<::Raiden::Physics::IPhysicsSystem> physicsSystem_;
  std::unique_ptr<::Raiden::Physics::PhysicsSyncSystem> physicsSync_;
  std::unique_ptr<ImGuiOverlay> overlay_;

  ::Raiden::Jobs::JobSystem jobSystem_;
  ::Raiden::Core::EngineConfig config_;
  ::Raiden::Engine::GamePluginLoader pluginLoader_;

  std::atomic<bool> running_{false};
  std::chrono::steady_clock::time_point lastFrameTime_;
  GizmoRenderCallback gizmoRenderCb_;
};

} // namespace Raiden::Engine
