#include <Raiden/Input/ActionMap.hpp>
#include <Raiden/Logger.hpp>

#include <SDL3/SDL.h>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>

namespace Raiden::Input {

using namespace ::Raiden::Core;
using namespace ::Raiden::Platform;

static const ::Raiden::Core::Logger s_logger("Raiden::Input::ActionMap");

// string-to-code helpers (SDL-based) for now

static ActionBinding::Source parseSource(std::string_view s) {
  if (s == "keyboard") {
    return ActionBinding::Keyboard;
  }

  if (s == "mouse_button") {
    return ActionBinding::MouseButton;
  }

  if (s == "gamepad_button") {
    return ActionBinding::GamepadButton;
  }

  if (s == "gamepad_axis") {
    return ActionBinding::GamepadAxis;
  }

  return ActionBinding::Keyboard;
}

static uint8_t parseMouseButton(std::string_view s) {
  if (s == "left") {
    return 0;
  }

  if (s == "middle") {
    return 1;
  }

  if (s == "right") {
    return 2;
  }

  return 0;
}

static uint8_t parseGamepadButton(std::string_view s) {
  static const std::unordered_map<std::string_view, uint8_t> kMap{
      {"a", SDL_GAMEPAD_BUTTON_SOUTH},
      {"b", SDL_GAMEPAD_BUTTON_EAST},
      {"x", SDL_GAMEPAD_BUTTON_WEST},
      {"y", SDL_GAMEPAD_BUTTON_NORTH},
      {"back", SDL_GAMEPAD_BUTTON_BACK},
      {"guide", SDL_GAMEPAD_BUTTON_GUIDE},
      {"start", SDL_GAMEPAD_BUTTON_START},
      {"leftstick", SDL_GAMEPAD_BUTTON_LEFT_STICK},
      {"rightstick", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
      {"leftshoulder", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
      {"rightshoulder", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
      {"dpup", SDL_GAMEPAD_BUTTON_DPAD_UP},
      {"dpdown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
      {"dpleft", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
      {"dpright", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
  };

  auto it = kMap.find(s);
  return it != kMap.end() ? it->second : static_cast<uint8_t>(SDL_GAMEPAD_BUTTON_SOUTH);
}

static uint8_t parseGamepadAxis(std::string_view s) {
  if (s == "leftx") {
    return SDL_GAMEPAD_AXIS_LEFTX;
  }

  if (s == "lefty") {
    return SDL_GAMEPAD_AXIS_LEFTY;
  }

  if (s == "rightx") {
    return SDL_GAMEPAD_AXIS_RIGHTX;
  }

  if (s == "righty") {
    return SDL_GAMEPAD_AXIS_RIGHTY;
  }

  if (s == "ltrigger") {
    return SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
  }

  if (s == "rtrigger") {
    return SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
  }

  return SDL_GAMEPAD_AXIS_LEFTX;
}

static uint8_t parseCode(ActionBinding::Source source, std::string_view s) {
  switch (source) {
  case ActionBinding::Keyboard:
    return static_cast<uint8_t>(
        SDL_GetScancodeFromName(std::string(s).c_str()));
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
    return 0.0F;
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
      const toml::array *bindingsArray = nullptr;

      if (val.is_array()) {
        // flat format: name = [ { source = ..., code = ... }, ... ]
        bindingsArray = val.as_array();
      } else if (val.is_table()) {
        // nested format: [name]\nbindings = [ { ... }, ... ]
        auto bindingsNode = (*val.as_table())["bindings"];
        if (bindingsNode.is_array()) {
          bindingsArray = bindingsNode.as_array();
        }
      }

      if (bindingsArray == nullptr) {
        continue;
      }

      Action action;
      for (const auto &elem : *bindingsArray) {
        if (!elem.is_table()) {
          continue;
        }

        const auto &b = *elem.as_table();
        ActionBinding binding;

        binding.source =
            parseSource(b["source"].value_or(std::string_view("")));
        binding.code =
            parseCode(binding.source, b["code"].value_or(std::string_view("")));
        binding.deadZone = b["dead_zone"].value_or(0.0F);
        binding.scale = b["scale"].value_or(1.0F);

        auto modStr = b["modifier"].value_or(std::string_view(""));
        binding.modifier = modStr.empty()
                               ? -1
                               : static_cast<int>(SDL_GetScancodeFromName(
                                     std::string(modStr).c_str()));

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
    action.value = 0.0F;

    for (auto &binding : action.bindings) {
      // check modifier
      if (binding.modifier >= 0 && !input.keysDown[binding.modifier]) {
        continue;
      }

      bool active = false;
      float v = 0.0F;

      switch (binding.source) {
      case ActionBinding::Keyboard:
        active = input.keysDown[binding.code];
        v = active ? 1.0F : 0.0F;
        break;

      case ActionBinding::MouseButton:
        active = binding.code < 3 && input.mouseButtons[binding.code];
        v = active ? 1.0F : 0.0F;
        break;

      case ActionBinding::GamepadButton:
        active = input.gamepadConnected &&
                 binding.code < InputState::kMaxGamepadButtons &&
                 input.gamepadButtons[binding.code];
        v = active ? 1.0F : 0.0F;
        break;

      case ActionBinding::GamepadAxis:
        v = getGamepadAxisValue(input, binding.code) * binding.scale;
        active = std::abs(v) > binding.deadZone;

        if (!active) {
          v = 0.0F;
        }

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

} // namespace Raiden::Input
