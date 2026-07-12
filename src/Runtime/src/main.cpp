#include <Raiden/Application.hpp>
#include <Raiden/Core/ConfigLoader.hpp>
#include <Raiden/Core/ConVar.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <Raiden/Platform/SDL3/SDL3Platform.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

static const Raiden::Core::Logger s_logger("main");

static void printUsage(const char *argv0) {
  s_logger.info("Usage: {} --datapack <path> [+convar value ...]", argv0);
}

int main(int argc, char *argv[]) {
  std::string datapackPath;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--datapack") == 0 && i + 1 < argc) {
      datapackPath = argv[++i];
    }
  }

  if (datapackPath.empty()) {
    s_logger.error("No datapack specified.");
    printUsage(argv[0]);
    return 1;
  }

  // resolve the datapack to an absolute path
  std::filesystem::path dp = std::filesystem::absolute(datapackPath);
  if (!std::filesystem::is_directory(dp)) {
    s_logger.error("Datapack directory not found: '{}'", dp.string());
    return 1;
  }

  // create VFS and mount the datapack
  auto vfs = Raiden::Core::createOSFileSystem();
  if (!vfs->mount("game://", dp.string())) {
    s_logger.error("Failed to mount datapack.");
    return 1;
  }

  auto platform = std::make_unique<Raiden::Platform::SDL3Platform>();
  auto device = std::make_unique<Raiden::Renderer::VulkanDevice>();

  Raiden::Core::EngineConfig config;
  config.renderBackend = Raiden::Core::RenderBackend::Vulkan;

  if (!Raiden::Core::loadConfig(*vfs, config)) {
    s_logger.warn("Falling back to defaults.");
  }

  // resolve plugin path relative to datapack
  std::string pluginPath = Raiden::Core::resolvePluginPath(config.plugin);
  std::string resolvedPluginPath;
  if (!pluginPath.empty()) {
    resolvedPluginPath = (dp / pluginPath).lexically_normal().string();
  }

  Raiden::Engine::Application app(std::move(platform), std::move(device),
                                  std::move(vfs));

  if (!app.init(config)) {
    return 1;
  }

  // apply CLI convar overrides after init (autoexec already loaded)
  Raiden::Core::convars().applyCLIOverrides(argc, argv);

  if (!resolvedPluginPath.empty()) {
    if (!app.loadGamePlugin(resolvedPluginPath)) {
      s_logger.warn("Game plugin not loaded, running without game.");
    }
  } else {
    s_logger.info("No game plugin configured, running without game.");
  }

  app.run();
  return 0;
}
