#include <RaidenEngineCore/Platform/SDL3/SDL3Platform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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

std::vector<const char *> SDL3Platform::getRequiredInstanceExtensions() const {
  uint32_t extensionCount = 0;
  char const *const *sdlExtensions =
      SDL_Vulkan_GetInstanceExtensions(&extensionCount);
  if (sdlExtensions == nullptr) {
    return {};
  }
  return std::vector<const char *>(sdlExtensions,
                                   sdlExtensions + extensionCount);
}

bool SDL3Platform::createVulkanSurface(VkInstance instance,
                                       VkSurfaceKHR *surface) {
  if (window_ == nullptr) {
    std::cerr << "createVulkanSurface failed: window is null\n";
    return false;
  }
  if (!SDL_Vulkan_CreateSurface(window_, instance, nullptr, surface)) {
    std::cerr << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError() << "\n";
    return false;
  }
  return true;
}

void SDL3Platform::getWindowSize(int &width, int &height) const {
  SDL_GetWindowSizeInPixels(window_, &width, &height);
}

} // namespace Raiden::Core
