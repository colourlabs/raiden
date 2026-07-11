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
  float scrollY = 0.0F;

  // gamepad
  bool gamepadConnected = false;
  float leftStickX = 0.0F;
  float leftStickY = 0.0F;
  float rightStickX = 0.0F;
  float rightStickY = 0.0F;
  float leftTrigger = 0.0F;
  float rightTrigger = 0.0F;
  bool gamepadButtons[kMaxGamepadButtons]{};

  void endFrame() {
    mouseDeltaX = 0;
    mouseDeltaY = 0;
    scrollY = 0.0F;
  }
};

} // namespace Raiden::Platform
