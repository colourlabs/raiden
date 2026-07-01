#pragma once

#include <RaidenEngineCore/Platform/IPlatform.hpp>

struct SDL_Window;
struct SDL_Gamepad;

namespace Raiden::Core {

class SDL3Platform : public IPlatform {
public:
  SDL3Platform() = default;
  ~SDL3Platform() override;

  SDL3Platform(const SDL3Platform &) = delete;
  SDL3Platform &operator=(const SDL3Platform &) = delete;
  SDL3Platform(SDL3Platform &&) = delete;
  SDL3Platform &operator=(SDL3Platform &&) = delete;

  bool init(const WindowConfig &config, RenderBackend backend) override;
  void shutdown() override;
  bool pollEvents() override;

  void *getNativeWindowHandle() override;

  void getWindowSize(int &width, int &height) const override;
  void setRelativeMouseMode(bool enabled) override;
  void endInputFrame() override;

  const InputState &getInputState() const override { return inputState_; }

  std::vector<const char *> getRequiredInstanceExtensions() const override;
  bool createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) override;

private:
  SDL_Window *window_ = nullptr;
  SDL_Gamepad *gamepad_ = nullptr;
  InputState inputState_{};
};

} // namespace Raiden::Core
