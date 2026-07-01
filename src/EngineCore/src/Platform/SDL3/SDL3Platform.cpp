#include <RaidenEngineCore/Platform/SDL3/SDL3Platform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <iostream>

namespace Raiden::Core {

SDL3Platform::~SDL3Platform() { SDL3Platform::shutdown(); }

bool SDL3Platform::init(const WindowConfig &config, RenderBackend backend) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
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
  if (gamepad_) {
    SDL_CloseGamepad(gamepad_);
    gamepad_ = nullptr;
  }

  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }

  SDL_Quit();
}

bool SDL3Platform::pollEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      return false;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      if (event.key.scancode < InputState::kMaxKeys) {
        inputState_.keysDown[event.key.scancode] =
            (event.type == SDL_EVENT_KEY_DOWN);
      }
      break;

    case SDL_EVENT_MOUSE_MOTION:
      inputState_.mouseX = event.motion.x;
      inputState_.mouseY = event.motion.y;
      inputState_.mouseDeltaX += event.motion.xrel;
      inputState_.mouseDeltaY += event.motion.yrel;
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      int idx = event.button.button - 1;
      if (idx >= 0 && idx < 3) {
        inputState_.mouseButtons[idx] =
            (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
      }
      break;
    }

    case SDL_EVENT_MOUSE_WHEEL:
      inputState_.scrollY = event.wheel.y;
      break;

    case SDL_EVENT_GAMEPAD_ADDED:
      if (!gamepad_) {
        gamepad_ = SDL_OpenGamepad(event.gdevice.which);
        inputState_.gamepadConnected = (gamepad_ != nullptr);
      }
      break;

    case SDL_EVENT_GAMEPAD_REMOVED:
      if (gamepad_ && event.gdevice.which == SDL_GetGamepadID(gamepad_)) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        inputState_.gamepadConnected = false;
      }
      break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      switch (event.gaxis.axis) {
      case SDL_GAMEPAD_AXIS_LEFTX:
        inputState_.leftStickX = event.gaxis.value / 32767.0f;
        break;
      case SDL_GAMEPAD_AXIS_LEFTY:
        inputState_.leftStickY = event.gaxis.value / 32767.0f;
        break;
      case SDL_GAMEPAD_AXIS_RIGHTX:
        inputState_.rightStickX = event.gaxis.value / 32767.0f;
        break;
      case SDL_GAMEPAD_AXIS_RIGHTY:
        inputState_.rightStickY = event.gaxis.value / 32767.0f;
        break;
      case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        inputState_.leftTrigger = event.gaxis.value / 32767.0f;
        break;
      case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        inputState_.rightTrigger = event.gaxis.value / 32767.0f;
        break;
      }
      break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      if (event.gbutton.button < InputState::kMaxGamepadButtons) {
        inputState_.gamepadButtons[event.gbutton.button] =
            (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
      }
      break;
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

void SDL3Platform::setRelativeMouseMode(bool enabled) {
  SDL_SetWindowRelativeMouseMode(window_, enabled);
}

void SDL3Platform::endInputFrame() {
  inputState_.endFrame();
}

} // namespace Raiden::Core
