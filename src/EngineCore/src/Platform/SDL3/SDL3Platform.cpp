#include <RaidenEngineCore/Platform/SDL3/SDL3Platform.hpp>

#include <SDL3/SDL.h>

#include <iostream>

namespace Raiden::Core {

SDL3Platform::~SDL3Platform() { shutdown(); }

bool SDL3Platform::init(const WindowConfig &config, RenderBackend backend) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
    return false;
  }

  Uint32 flags = (backend == RenderBackend::Vulkan) ? SDL_WINDOW_VULKAN
                                                    : SDL_WINDOW_OPENGL;

  if (config.resizable) {
    flags |= SDL_WINDOW_RESIZABLE;
  }

  if (config.fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
  }

  window_ = SDL_CreateWindow(config.title.c_str(), config.width, config.height,
                             flags);
  if (window_ == nullptr) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
    return false;
  }

  return true;
}

void SDL3Platform::shutdown() {
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }

  SDL_Quit();
}

bool SDL3Platform::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      return false;
    }
  }
  return true;
}

void *SDL3Platform::getNativeWindowHandle() { return window_; }

} // namespace Raiden::Core
