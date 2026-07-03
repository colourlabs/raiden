#pragma once

namespace Raiden::Platform {

struct InputState {
  static constexpr int kMaxKeys = 256;
  static constexpr int kMaxGamepadButtons = 16;

  // keyboard
  bool keysDown[kMaxKeys]{};

  // mouse
  int mouseX = 0;
  int mouseY = 0;
  int mouseDeltaX = 0;
  int mouseDeltaY = 0;
  bool mouseButtons[3]{};
  float scrollY = 0.0f;

  // gamepad
  bool gamepadConnected = false;
  float leftStickX = 0.0f;
  float leftStickY = 0.0f;
  float rightStickX = 0.0f;
  float rightStickY = 0.0f;
  float leftTrigger = 0.0f;
  float rightTrigger = 0.0f;
  bool gamepadButtons[kMaxGamepadButtons]{};

  void endFrame() {
    mouseDeltaX = 0;
    mouseDeltaY = 0;
    scrollY = 0.0f;
  }
};

} // namespace Raiden::Platform
