#include <RaidenEngineCore/Application.hpp>
#include <RaidenEngineCore/Core/IVirtualFileSystem.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <RaidenEngineCore/Platform/SDL3/SDL3Platform.hpp>
#include <RaidenEngineCore/Renderer/Vulkan/VulkanDevice.hpp>

#include <toml++/toml.hpp>

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

static const Raiden::Core::Logger s_logger("main");

static bool loadConfig(Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Core::EngineConfig &outConfig,
                       std::string &outPluginPath) {
  try {
    std::string content = vfs.readAll("game://engine.toml");
    if (content.empty()) {
      s_logger.warn("engine.toml not found in datapack.");
      return false;
    }

    auto table = toml::parse(content);

    if (auto win = table["window"].as_table()) {
      outConfig.window.title =
          (*win)["title"].value_or(std::string("raiden engine"));
      outConfig.window.width = (*win)["width"].value_or(1280);
      outConfig.window.height = (*win)["height"].value_or(720);
      outConfig.window.resizable = (*win)["resizable"].value_or(true);
      outConfig.window.fullscreen = (*win)["fullscreen"].value_or(false);
      outConfig.window.vsync = (*win)["vsync"].value_or(true);
    }

    if (auto rend = table["render"].as_table()) {
      auto backend = (*rend)["backend"].value_or(std::string("vulkan"));
      outConfig.renderBackend = (backend == "vulkan")
                                    ? Raiden::Core::RenderBackend::Vulkan
                                    : Raiden::Core::RenderBackend::Vulkan;
      outConfig.enableValidation = (*rend)["validation"].value_or(false);
    }

    if (auto game = table["game"].as_table()) {
      outPluginPath = (*game)["plugin"].value_or(std::string(""));
    }

    return true;
  } catch (const toml::parse_error &err) {
    s_logger.error("Failed to parse engine.toml: {}", err.description());
    return false;
  }
}

static void printUsage(const char *argv0) {
  s_logger.info("Usage: {} --datapack <path>", argv0);
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

  auto platform = std::make_unique<Raiden::Core::SDL3Platform>();
  auto device = std::make_unique<Raiden::Core::VulkanDevice>();

  Raiden::Core::EngineConfig config;
  std::string pluginPath;

  if (!loadConfig(*vfs, config, pluginPath)) {
    s_logger.warn("Falling back to defaults.");
  }

  // resolve plugin path relative to datapack
  std::string resolvedPluginPath;
  if (!pluginPath.empty()) {
    resolvedPluginPath = (dp / pluginPath).lexically_normal().string();
  }

  Raiden::Core::Application app(std::move(platform), std::move(device),
                                std::move(vfs));

  if (!app.init(config)) {
    return 1;
  }

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
