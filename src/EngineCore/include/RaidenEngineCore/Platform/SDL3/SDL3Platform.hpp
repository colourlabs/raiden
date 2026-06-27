#pragma once

#include <RaidenEngineCore/Platform/IPlatform.hpp>

struct SDL_Window;

namespace Raiden::Core {

class SDL3Platform final : public IPlatform {
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

private:
  SDL_Window *window_ = nullptr;
};

} // namespace Raiden::Core
