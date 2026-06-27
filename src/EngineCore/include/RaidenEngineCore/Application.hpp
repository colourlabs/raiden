#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>
#include <RaidenEngineCore/Platform/IPlatform.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>

#include <memory>

namespace Raiden::Core {

class Application {
public:
  Application(std::unique_ptr<IPlatform> platform,
              std::unique_ptr<IRenderDevice> device);
  ~Application();

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;
  Application(Application &&) = delete;
  Application &operator=(Application &&) = delete;

  bool init(const EngineConfig &config);
  void shutdown();
  void run();

  [[nodiscard]] IPlatform &getPlatform() const { return *platform_; }
  [[nodiscard]] IRenderDevice &getDevice() const { return *device_; }

private:
  std::unique_ptr<IPlatform> platform_;
  std::unique_ptr<IRenderDevice> device_;
  EngineConfig config_;
  bool running_ = false;
};

} // namespace Raiden::Core
