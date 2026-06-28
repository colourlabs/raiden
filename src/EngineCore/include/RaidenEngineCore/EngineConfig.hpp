#pragma once

#include <string>

namespace Raiden::Core {

enum class RenderBackend {
  Vulkan,

  // TODO: maybe OpenGL ES in the future?
};

struct WindowConfig {
  std::string title = "raiden example game";
  int width = 800;
  int height = 600;
  bool resizable = true;
  bool fullscreen = false;
  bool vsync = true;
};

struct AudioConfig {
  float masterVolume = 1.0f;
};

struct PluginConfig {
  std::string fallback;
  std::string windows;
  std::string macosArm;
  std::string macosX86;
  std::string linux_;
};

struct EngineConfig {
  WindowConfig window;
  AudioConfig audio;
  RenderBackend renderBackend = RenderBackend::Vulkan;

  bool enableValidation = false;
  PluginConfig plugin;
};

} // namespace Raiden::Core