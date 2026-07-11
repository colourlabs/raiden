#pragma once

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Platform/InputState.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Raiden::Input {

struct ActionBinding {
  enum Source : uint8_t {
    Keyboard,
    MouseButton,
    GamepadButton,
    GamepadAxis,
  };

  Source source;
  uint8_t code;
  float deadZone = 0.0F;
  float scale = 1.0F;
  int modifier = -1; // scancode that must be held, -1 = none
};

struct Action {
  std::vector<ActionBinding> bindings;

  // runtime state (filled by evaluate())
  bool pressed = false;
  bool justPressed = false;
  bool justReleased = false;
  float value = 0.0F;
};

class ActionMap {
public:
  ActionMap() = default;

  bool loadFromFile(::Raiden::Core::IVirtualFileSystem &vfs, std::string_view path);
  bool loadFromString(std::string_view tomlSource);

  void evaluate(const ::Raiden::Platform::InputState &input);

  void registerAction(std::string name, Action action);

  const Action *find(std::string_view name) const;
  Action &operator[](std::string_view name);

  bool contains(std::string_view name) const;

private:
  std::unordered_map<std::string, Action> actions_;
};

} // namespace Raiden::Input
