#include <Raiden/Application.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <toml++/toml.hpp>

#include <Raiden/Platform/SDL3/SDL3Platform.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>

#include <RaidenEditor/EditorPlugin.hpp>

#include <cstring>
#include <filesystem>
#include <memory>

static const Raiden::Core::Logger s_logger("EditorRuntime");

static bool loadConfig(Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Core::EngineConfig &outConfig) {
  try {
    std::string content = vfs.readAll("game://engine.toml");
    if (content.empty()) {
      s_logger.warn("engine.toml not found in datapack.");
      return false;
    }

    auto table = toml::parse(content);

    if (auto *win = table["window"].as_table()) {
      outConfig.window.title =
          (*win)["title"].value_or(std::string("Raiden Editor"));
      outConfig.window.width = (*win)["width"].value_or(1280);
      outConfig.window.height = (*win)["height"].value_or(720);
      outConfig.window.resizable = (*win)["resizable"].value_or(true);
      outConfig.window.fullscreen = (*win)["fullscreen"].value_or(false);
      outConfig.window.vsync = (*win)["vsync"].value_or(true);
    }

    if (auto *rend = table["render"].as_table()) {
      outConfig.enableValidation = (*rend)["validation"].value_or(false);

      auto aa = (*rend)["antialiasing"].value_or(std::string("none"));
      if (aa == "msaa_x2") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx2;
      } else if (aa == "msaa_x4") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx4;
      } else if (aa == "msaa_x8") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx8;
      } else {
        outConfig.antialiasing = Raiden::Core::Antialiasing::None;
      }
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

  std::filesystem::path dp = std::filesystem::absolute(datapackPath);
  if (!std::filesystem::is_directory(dp)) {
    s_logger.error("Datapack directory not found: '{}'", dp.string());
    return 1;
  }

  auto vfs = Raiden::Core::createOSFileSystem();
  if (!vfs->mount("game://", dp.string())) {
    s_logger.error("Failed to mount datapack.");
    return 1;
  }

  auto platform = std::make_unique<Raiden::Platform::SDL3Platform>();
  auto device = std::make_unique<Raiden::Renderer::VulkanDevice>();

  Raiden::Core::EngineConfig config;

  if (!loadConfig(*vfs, config)) {
    s_logger.warn("Falling back to defaults.");
  }

  Raiden::Engine::Application app(std::move(platform), std::move(device),
                                  std::move(vfs));

  if (!app.init(config)) {
    return 1;
  }

  auto editorPlugin = std::make_unique<RaidenEditor::EditorPlugin>();
  if (!app.registerPlugin(editorPlugin.release())) {
    s_logger.error("Failed to register editor plugin.");
    return 1;
  }

  app.run();
  return 0;
}
