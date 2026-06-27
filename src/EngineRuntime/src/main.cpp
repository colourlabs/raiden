#include <RaidenEngineCore/Application.hpp>
#include <RaidenEngineCore/Platform/SDL3/SDL3Platform.hpp>
#include <RaidenEngineCore/Renderer/VulkanDevice.hpp>

#include <memory>

int main() {
  auto platform = std::make_unique<Raiden::Core::SDL3Platform>();
  auto device = std::make_unique<Raiden::Core::VulkanDevice>();

  Raiden::Core::Application app(std::move(platform), std::move(device));

  Raiden::Core::EngineConfig config;
  config.enableValidation = true;

  if (!app.init(config)) {
    return 1;
  }

  app.run();

  return 0;
}
