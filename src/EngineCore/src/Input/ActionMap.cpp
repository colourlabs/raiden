#include <RaidenEngineCore/Input/ActionMap.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <SDL3/SDL.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::ActionMap");

// string-to-code helpers (SDL-based) for now

static ActionBinding::Source parseSource(std::string_view s) {
  if (s == "keyboard")
    return ActionBinding::Keyboard;
  if (s == "mouse_button")
    return ActionBinding::MouseButton;
  if (s == "gamepad_button")
    return ActionBinding::GamepadButton;
  if (s == "gamepad_axis")
    return ActionBinding::GamepadAxis;
  return ActionBinding::Keyboard;
}

static uint8_t parseMouseButton(std::string_view s) {
  if (s == "left")
    return 0;
  if (s == "middle")
    return 1;
  if (s == "right")
    return 2;
  return 0;
}

static uint8_t parseGamepadButton(std::string_view s) {
  if (s == "a")
    return SDL_GAMEPAD_BUTTON_SOUTH;
  if (s == "b")
    return SDL_GAMEPAD_BUTTON_EAST;
  if (s == "x")
    return SDL_GAMEPAD_BUTTON_WEST;
  if (s == "y")
    return SDL_GAMEPAD_BUTTON_NORTH;
  if (s == "back")
    return SDL_GAMEPAD_BUTTON_BACK;
  if (s == "guide")
    return SDL_GAMEPAD_BUTTON_GUIDE;
  if (s == "start")
    return SDL_GAMEPAD_BUTTON_START;
  if (s == "leftstick")
    return SDL_GAMEPAD_BUTTON_LEFT_STICK;
  if (s == "rightstick")
    return SDL_GAMEPAD_BUTTON_RIGHT_STICK;
  if (s == "leftshoulder")
    return SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
  if (s == "rightshoulder")
    return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
  if (s == "dpup")
    return SDL_GAMEPAD_BUTTON_DPAD_UP;
  if (s == "dpdown")
    return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
  if (s == "dpleft")
    return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
  if (s == "dpright")
    return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
  return SDL_GAMEPAD_BUTTON_SOUTH;
}

static uint8_t parseGamepadAxis(std::string_view s) {
  if (s == "leftx")
    return SDL_GAMEPAD_AXIS_LEFTX;
  if (s == "lefty")
    return SDL_GAMEPAD_AXIS_LEFTY;
  if (s == "rightx")
    return SDL_GAMEPAD_AXIS_RIGHTX;
  if (s == "righty")
    return SDL_GAMEPAD_AXIS_RIGHTY;
  if (s == "ltrigger")
    return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
  if (s == "rtrigger")
    return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  return SDL_GAMEPAD_AXIS_LEFTX;
}

static uint8_t parseCode(ActionBinding::Source source, std::string_view s) {
  switch (source) {
  case ActionBinding::Keyboard:
    return static_cast<uint8_t>(SDL_GetScancodeFromName(s.data()));
  case ActionBinding::MouseButton:
    return parseMouseButton(s);
  case ActionBinding::GamepadButton:
    return parseGamepadButton(s);
  case ActionBinding::GamepadAxis:
    return parseGamepadAxis(s);
  }
  return 0;
}

static float getGamepadAxisValue(const InputState &input, uint8_t axis) {
  switch (axis) {
  case SDL_GAMEPAD_AXIS_LEFTX:
    return input.leftStickX;
  case SDL_GAMEPAD_AXIS_LEFTY:
    return input.leftStickY;
  case SDL_GAMEPAD_AXIS_RIGHTX:
    return input.rightStickX;
  case SDL_GAMEPAD_AXIS_RIGHTY:
    return input.rightStickY;
  case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
    return input.leftTrigger;
  case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
    return input.rightTrigger;
  default:
    return 0.0f;
  }
}

// ActionMap

bool ActionMap::loadFromFile(IVirtualFileSystem &vfs, std::string_view path) {
  std::string content = vfs.readAll(path);
  if (content.empty() && !vfs.exists(path)) {
    s_logger.error("File not found: '{}'", path);
    return false;
  }
  return loadFromString(content);
}

bool ActionMap::loadFromString(std::string_view tomlSource) {
  try {
    auto table = toml::parse(tomlSource);

    for (auto &[key, val] : table) {
      if (!val.is_array())
        continue;

      Action action;
      for (auto &elem : *val.as_array()) {
        if (!elem.is_table())
          continue;

        auto &b = *elem.as_table();
        ActionBinding binding;

        binding.source =
            parseSource(b["source"].value_or(std::string_view("")));
        binding.code =
            parseCode(binding.source, b["code"].value_or(std::string_view("")));
        binding.deadZone = b["dead_zone"].value_or(0.0f);
        binding.scale = b["scale"].value_or(1.0f);

        auto modStr = b["modifier"].value_or(std::string_view(""));
        binding.modifier =
            modStr.empty()
                ? -1
                : static_cast<int>(SDL_GetScancodeFromName(modStr.data()));

        action.bindings.push_back(binding);
      }

      actions_[std::string(key)] = std::move(action);
    }

    s_logger.info("Loaded {} actions from string", actions_.size());
    return true;
  } catch (const toml::parse_error &err) {
    s_logger.error("Failed to parse TOML: {}", err.description());
    return false;
  }
}

void ActionMap::evaluate(const InputState &input) {
  for (auto &[name, action] : actions_) {
    bool prevPressed = action.pressed;
    action.pressed = false;
    action.value = 0.0f;

    for (auto &binding : action.bindings) {
      // check modifier
      if (binding.modifier >= 0 && !input.keysDown[binding.modifier]) {
        continue;
      }

      bool active = false;
      float v = 0.0f;

      switch (binding.source) {
      case ActionBinding::Keyboard:
        active = input.keysDown[binding.code];
        v = active ? 1.0f : 0.0f;
        break;

      case ActionBinding::MouseButton:
        active = binding.code < 3 && input.mouseButtons[binding.code];
        v = active ? 1.0f : 0.0f;
        break;

      case ActionBinding::GamepadButton:
        active = input.gamepadConnected &&
                 binding.code < InputState::kMaxGamepadButtons &&
                 input.gamepadButtons[binding.code];
        v = active ? 1.0f : 0.0f;
        break;

      case ActionBinding::GamepadAxis:
        v = getGamepadAxisValue(input, binding.code) * binding.scale;
        active = std::abs(v) > binding.deadZone;
        if (!active)
          v = 0.0f;
        break;
      }

      if (active) {
        action.pressed = true;
        action.value = std::max(action.value, v);
      }
    }

    action.justPressed = action.pressed && !prevPressed;
    action.justReleased = !action.pressed && prevPressed;
  }
}

void ActionMap::registerAction(std::string name, Action action) {
  actions_[std::move(name)] = std::move(action);
}

const Action *ActionMap::find(std::string_view name) const {
  auto it = actions_.find(std::string(name));
  return it != actions_.end() ? &it->second : nullptr;
}

Action &ActionMap::operator[](std::string_view name) {
  return actions_[std::string(name)];
}

bool ActionMap::contains(std::string_view name) const {
  return actions_.contains(std::string(name));
}

} // namespace Raiden::Core
