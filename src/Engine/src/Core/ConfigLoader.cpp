#include <Raiden/Core/ConfigLoader.hpp>
#include <Raiden/Logger.hpp>

#include <toml++/toml.hpp>

static const ::Raiden::Core::Logger s_logger("ConfigLoader");

namespace Raiden::Core {

bool loadConfig(IVirtualFileSystem &vfs, EngineConfig &outConfig,
                const std::string &defaultTitle) {
  try {
    std::string content = vfs.readAll("game://engine.toml");
    if (content.empty()) {
      s_logger.warn("engine.toml not found in datapack.");
      return false;
    }

    auto table = toml::parse(content);

    if (auto *win = table["window"].as_table()) {
      outConfig.window.title =
          (*win)["title"].value_or(defaultTitle);
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
        outConfig.antialiasing = Antialiasing::MSAAx2;
      } else if (aa == "msaa_x4") {
        outConfig.antialiasing = Antialiasing::MSAAx4;
      } else if (aa == "msaa_x8") {
        outConfig.antialiasing = Antialiasing::MSAAx8;
      } else {
        outConfig.antialiasing = Antialiasing::None;
      }
    }

    if (auto *game = table["game"].as_table()) {
      outConfig.plugin.fallback = (*game)["plugin"].value_or(std::string(""));

      if (auto *plat = (*game)["platform"].as_table()) {
        outConfig.plugin.windows = (*plat)["windows"].value_or(std::string(""));
        outConfig.plugin.macosArm =
            (*plat)["macos_arm"].value_or(std::string(""));
        outConfig.plugin.macosX86 =
            (*plat)["macos_x86"].value_or(std::string(""));
        outConfig.plugin.linux_ = (*plat)["linux"].value_or(std::string(""));
      }
    }

    return true;
  } catch (const toml::parse_error &err) {
    s_logger.error("Failed to parse engine.toml: {}", err.description());
    return false;
  }
}

std::string resolvePluginPath(const PluginConfig &cfg) {
#ifdef _WIN32
  return cfg.windows.empty() ? cfg.fallback : cfg.windows;
#elif defined(__APPLE__)
#if defined(__arm64__) || defined(__aarch64__)
  return cfg.macosArm.empty() ? cfg.fallback : cfg.macosArm;
#else
  return cfg.macosX86.empty() ? cfg.fallback : cfg.macosX86;
#endif
#elif defined(__linux__)
  return cfg.linux_.empty() ? cfg.fallback : cfg.linux_;
#else
  return cfg.fallback;
#endif
}

} // namespace Raiden::Core
