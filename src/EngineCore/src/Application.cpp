#include <RaidenEngineCore/Application.hpp>

// TODO: use enginewide logger instead of just std::cerr piss

namespace Raiden::Core {

Application::Application(std::unique_ptr<IPlatform> platform,
                         std::unique_ptr<IRenderDevice> device)
    : platform_(std::move(platform)), device_(std::move(device)) {}

Application::~Application() { shutdown(); }

bool Application::init(const EngineConfig &config) {
  config_ = config;

  if (!platform_->init(config_.window, config_.renderBackend)) {
    return false;
  }

  if (!device_->init(config_, platform_->getNativeWindowHandle())) {
    return false;
  }

  running_ = true;
  return true;
}

void Application::shutdown() {
  running_ = false;
  device_->shutdown();
  platform_->shutdown();
}

void Application::run() {
  while (running_) {
    if (!platform_->pollEvents()) {
      running_ = false;
    }
  }
}

} // namespace Raiden::Core
