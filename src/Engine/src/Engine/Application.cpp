#include <Raiden/Application.hpp>
#include <Raiden/Assets/AssetManager.hpp>
#include <Raiden/Audio/OpenALDevice.hpp>
#include <Raiden/Core/ConVar.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/Engine/VulkanImGuiBackend.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Renderer/Vulkan/IVulkanRenderDevice.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>

#include "Physics/JoltPhysicsSystem.hpp"

#include <array>
#include <cstring>
#include <functional>

#include <glm/gtc/matrix_transform.hpp>

namespace Raiden::Engine {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;
using namespace ::Raiden::Platform;
using namespace ::Raiden::Assets;
using namespace ::Raiden::Audio;
using namespace ::Raiden::Physics;

static const ::Raiden::Core::Logger s_logger("Raiden::Engine::Application");

Application::Application(std::unique_ptr<IPlatform> platform,
                         std::unique_ptr<IRenderDevice> device,
                         std::unique_ptr<IVirtualFileSystem> vfs)
    : platform_(std::move(platform)), device_(std::move(device)),
      vfs_(std::move(vfs)) {
  lastFrameTime_ = std::chrono::steady_clock::now();
}

Application::~Application() { shutdown(); }

bool Application::init(const EngineConfig &config) {
  config_ = config;

  s_logger.info("Initializing application...");

  // register core convars with sane defaults
  auto &cv = Core::convars();
  cv.registerInt("tick_rate", 60, Core::ConVarNone, "Fixed tick rate in Hz");
  cv.registerBool("show_fps", false, Core::ConVarArchive, "Show FPS counter");
  cv.registerBool("show_frame_time", false, Core::ConVarArchive,
                  "Show frame time graph");
  cv.registerBool("show_draw_calls", false, Core::ConVarArchive,
                  "Show draw calls & triangles");
  cv.registerBool("show_gpu_stats", false, Core::ConVarArchive,
                  "Show GPU profiler");
  cv.registerBool("show_convars", false, Core::ConVarNone,
                  "Show ConVars debug window");
  cv.registerFloat("mouse_sensitivity", 0.002F, Core::ConVarArchive,
                   "Mouse look sensitivity");
  cv.registerFloat("camera_speed", 3.0F, Core::ConVarArchive,
                   "Camera movement speed");
  cv.registerBool("vsync", config.window.vsync, Core::ConVarArchive,
                  "Vertical sync");
  cv.registerInt("window_width", config.window.width, Core::ConVarArchive,
                 "Window width");
  cv.registerInt("window_height", config.window.height, Core::ConVarArchive,
                 "Window height");
  cv.registerBool("fullscreen", config.window.fullscreen, Core::ConVarArchive,
                  "Fullscreen mode");
  cv.registerBool("validation", config.enableValidation,
                  Core::ConVarRestart, "Vulkan validation layers");
  cv.registerString("scene", "default.scene.json", Core::ConVarArchive,
                    "Scene file to load");
  cv.registerFloat("master_volume", config.audio.masterVolume,
                   Core::ConVarArchive, "Master audio volume (0-1)");
  cv.registerFloat("physics_gravity", -9.81F, Core::ConVarArchive,
                   "Physics gravity Y");
  cv.registerBool("physics_enabled", true, Core::ConVarArchive,
                  "Enable physics simulation");

  // load autoexec.cfg (overrides defaults)
  if (vfs_) {
    cv.loadAutoExec(*vfs_);
  }

  // CLI overrides (+convarname value) override everything
  // (applied later in main.cpp after argc/argv are available)

  jobSystem_.init();

  if (!platform_->init(config_.window, config_.renderBackend)) {
    s_logger.error("Failed to initialize platform.");
    return false;
  }

  if (!device_->init(config_, platform_.get())) {
    s_logger.error("Failed to initialize render device.");
    return false;
  }

  device_->setJobSystem(jobSystem_);

  assetManager_ = std::make_unique<AssetManager>(*device_, *vfs_);
  assetManager_->setJobSystem(jobSystem_);

  audioDevice_ = std::make_unique<OpenALDevice>();
  audioDevice_->setJobSystem(jobSystem_);
  if (!audioDevice_->init(config_.audio, *vfs_)) {
    s_logger.warn("Failed to initialize audio device.");
    audioDevice_.reset();
  }

  physicsSystem_ = std::make_unique<JoltPhysicsSystem>();
  if (!physicsSystem_->init(config_.physics)) {
    s_logger.warn("Failed to initialize physics system.");
    physicsSystem_.reset();
  }

  if (physicsSystem_) {
    physicsSync_ = std::make_unique<PhysicsSyncSystem>(*physicsSystem_);
  }

  // init ImGui overlay
  if (auto *vkDevice = dynamic_cast<IVulkanRenderDevice *>(device_.get())) {
    auto backend = std::make_unique<VulkanImGuiBackend>(
        *vkDevice, vkDevice->getRenderPass(),
        vkDevice->getSwapchainImageCount());
    overlay_ = std::make_unique<ImGuiOverlay>();
    overlay_->init(std::move(backend));
  }

  s_logger.info("Application initialized successfully.");
  running_ = true;
  return true;
}

static bool initPlugin(GamePluginLoader &loader,
                        IRenderDevice &device,
                        IVirtualFileSystem &vfs,
                        IAssetManager &assets,
                        IPlatform *platform,
                        IAudioDevice *audio,
                        ::Raiden::Physics::IPhysicsSystem *physics) {
  if (!loader.plugin().init(device, vfs, assets, platform, audio, physics)) {
    s_logger.error("Game plugin init failed.");
    loader.unload();
    return false;
  }

  auto *vkDev = dynamic_cast<IVulkanRenderDevice *>(&device);
  if (vkDev != nullptr) {
    if (auto *world = loader.plugin().getWorld()) {
      vkDev->setWorld(world);
    }
  }

  s_logger.info("Game plugin '{}' initialized.", loader.plugin().name());
  return true;
}

bool Application::loadGamePlugin(std::string_view path) {
  if (!pluginLoader_.load(path)) {
    s_logger.error("Failed to load game plugin.");
    return false;
  }

  return initPlugin(pluginLoader_, *device_, *vfs_, *assetManager_,
                    platform_.get(), audioDevice_.get(), physicsSystem_.get());
}

bool Application::registerPlugin(Raiden::Engine::IGamePlugin *plugin) {
  pluginLoader_.registerPlugin(plugin);
  return initPlugin(pluginLoader_, *device_, *vfs_, *assetManager_,
                    platform_.get(), audioDevice_.get(), physicsSystem_.get());
}

void Application::requestShutdown() { running_ = false; }

void Application::shutdown() {
  if (!running_) {
    return;
  }
  running_ = false;

  s_logger.info("Shutting down application...");

  // save archived convars to autoexec.cfg
  if (vfs_) {
    Core::convars().saveAutoExec(*vfs_);
  }

  overlay_.reset();
  pluginLoader_.unload();
  assetManager_.reset();
  physicsSync_.reset();
  physicsSystem_.reset();
  audioDevice_.reset();
  jobSystem_.shutdown();
  device_->shutdown();
  platform_->shutdown();
}

void Application::run() {
  s_logger.info("Entering main loop.");

  if (!pluginLoader_.isLoaded()) {
    s_logger.warn("No game plugin loaded, running with default scene.");
  }

  // shadow callback: only game geometry, no overlay/gizmo (they use raw Vulkan
  // pipeline binds that bypass the command buffer shadow mode interception)
  device_->setShadowCallback(
      [this](ICommandBuffer &cmd, uint32_t wi, uint32_t) {
        if (wi == 0 && pluginLoader_.isLoaded()) {
          pluginLoader_.plugin().render(cmd);
        }
      });

  while (running_) {
    auto now = std::chrono::steady_clock::now();
    float frameDelta =
        std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    // clamp to avoid spiral of death
    if (frameDelta > 0.25F) {
      frameDelta = 0.25F;
    }

    // read tick rate from convar (can be changed at runtime)
    int tickRateHz = Core::convars().getInt("tick_rate", 60);
    tickRate_ = 1.0F / static_cast<float>(tickRateHz);

    if (!platform_->pollEvents()) {
      break;
    }

    // fixed timestep accumulator for game logic and physics
    accumulator_ += frameDelta;

    // apply gravity from convar if changed
    if (physicsSystem_ && physicsSystemLastGravity_ !=
                             Core::convars().getFloat("physics_gravity", -9.81F)) {
      physicsSystemLastGravity_ =
          Core::convars().getFloat("physics_gravity", -9.81F);
      physicsSystem_->setGravity({0.0F, physicsSystemLastGravity_, 0.0F});
    }

    while (accumulator_ >= tickRate_) {
      if (pluginLoader_.isLoaded()) {
        pluginLoader_.plugin().update(tickRate_, platform_->getInputState());
      }

      if (Core::convars().getBool("physics_enabled", true)) {
        if (physicsSync_) {
          auto *world = getWorld();
          if (world != nullptr) {
            physicsSync_->update(*world, tickRate_);
          }
        }

        if (physicsSystem_) {
          physicsSystem_->step(tickRate_);
        }
      }

      accumulator_ -= tickRate_;
    }

    // interpolation alpha for rendering (0..1 between ticks)
    // float alpha = accumulator_ / tickRate_;

    if (audioDevice_) {
      audioDevice_->processPendingLoads();
    }

    assetManager_->processLoadQueue();

    if (overlay_) {
      int w = 0, h = 0;
      platform_->getWindowSize(w, h);

      // sync convar state to overlay
      if (Core::convars().getBool("show_convars")) {
        overlay_->toggleConVarsWindow();
        Core::convars().setBool("show_convars", false);
      }

      ProfilerFrameData profiler{};
      profiler.cpuFrameTimeMs = frameDelta * 1000.0F;

      if (auto *vkDev = dynamic_cast<IVulkanRenderDevice *>(device_.get())) {
        const auto &d = dynamic_cast<const VulkanDevice &>(*vkDev);
        profiler.gpuFrameTimeMs = d.gpuTimeMs();
        profiler.drawCalls = d.lastDrawCalls();
        profiler.triangles = d.lastTriangles();
      }

      std::function<void()> pluginUI;
      if (pluginLoader_.isLoaded()) {
        pluginUI = [&]() { pluginLoader_.plugin().onDebugUI(); };
      }

      // pass active camera matrices to overlay for ImGui
      if (auto *world = getWorld()) {
        std::array<float, 16> viewMat = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        std::array<float, 16> projMat = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        world->view<Raiden::ECS::Camera>().each(
            [&](Raiden::ECS::Entity, Raiden::ECS::Camera &cam) {
              if (cam.active) {
                std::memcpy(viewMat.data(), &cam.view, sizeof(float) * 16);
                float aspect = static_cast<float>(w) /
                               static_cast<float>(h > 1 ? h : 1);
                auto proj = glm::perspective(glm::radians(cam.fov), aspect,
                                             cam.zNear, cam.zFar);
                proj[1][1] *= -1.0F;
                // Vulkan depth remap
                proj[2][2] = (0.5F * proj[2][2]) + (0.5F * proj[2][3]);
                proj[3][2] = (0.5F * proj[3][2]) + (0.5F * proj[3][3]);
                std::memcpy(projMat.data(), &proj, sizeof(float) * 16);
              }
            });
        overlay_->setCameraMatrices(viewMat.data(), projMat.data(), w, h);
      }

      overlay_->newFrame(platform_->getInputState(), w, h, frameDelta,
                         profiler, pluginUI);
      Raiden::Engine::ImGuiOverlay::endFrame();
    }

    device_->drawFrame([&](ICommandBuffer &cmd, uint32_t wi, uint32_t n) {
      // worker 0 handles game rendering
      if (wi == 0 && pluginLoader_.isLoaded()) {
        pluginLoader_.plugin().render(cmd);
      }

      // gizmo rendering (between game and overlay)
      if (wi == 0 && gizmoRenderCb_) {
        gizmoRenderCb_(cmd);
      }

      // worker n-1 handles overlay (if any workers remain)
      if (n > 1 && wi == n - 1 && overlay_) {
        overlay_->renderDrawData(cmd);
      }

      // single-threaded fallback: everything on worker 0
      if (n == 1 && wi == 0 && overlay_) {
        overlay_->renderDrawData(cmd);
      }
    });

    // end the ImGui frame even if drawFrame skipped the callback (e.g. resize)
    if (overlay_) {
      Raiden::Engine::ImGuiOverlay::endFrame();
    }

    platform_->endInputFrame();
  }

  s_logger.info("Exited main loop.");
}

} // namespace Raiden::Engine
