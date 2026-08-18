#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Engine/DebugConsole.hpp>

#include <imgui.h>

#include <sstream>

namespace Raiden::Engine {

void DebugConsole::init() {
  addCommand("help", [](const std::string &) {
    std::ostringstream ss;
    ss << "=== debug console cxommands ===\n";
    ss << "  help              Show this help\n";
    ss << "  clear             Clear output\n";
    ss << "  list_convars      List all registered ConVars\n";
    ss << "  set <name> <val>  Set a ConVar value\n";
    ss << "  get <name>        Get a ConVar value\n";
    ss << "  entities          List all entities\n";
    ss << "  entity <index>    Show entity details\n";
    ss << "\n";
    ss << "Type any ConVar name to toggle/set it.\n";
    ss << "TAB for autocomplete, UP/DOWN for history.";
    return ss.str();
  });

  addCommand("clear", [this](const std::string &) {
    outputLines_.clear();
    return "";
  });

  addCommand("list_convars", [](const std::string &) {
    std::ostringstream ss;
    ss << "=== ConVars ===\n";
    auto all = Core::convars().getAll();
    for (const auto &cv : all) {
      if (cv.flags & Core::ConVarHidden) {
        continue;
      }
      ss << "  " << cv.name << " = ";
      std::visit([&](auto &v) { ss << v; }, cv.value);
      if (!cv.description.empty()) {
        ss << "  // " << cv.description;
      }
      ss << "\n";
    }
    return ss.str();
  });

  addCommand("set", [](const std::string &args) {
    auto space = args.find(' ');
    if (space == std::string::npos) {
      return std::string("Usage: set <convar> <value>");
    }
    auto name = args.substr(0, space);
    auto value = args.substr(space + 1);
    auto &cv = Core::convars();
    if (!cv.exists(name)) {
      return "Unknown ConVar: " + name;
    }
    cv.setFromString(name, value);
    return name + " = " + cv.getString(name, value);
  });

  addCommand("get", [](const std::string &args) {
    auto &cv = Core::convars();
    if (!cv.exists(args)) {
      return "Unknown ConVar: " + args;
    }
    return args + " = " + cv.getString(args, "");
  });

  addCommand("entities", [this](const std::string &) {
    if (!world_) {
      return std::string("No world available");
    }
    std::ostringstream ss;
    ss << "=== Entities ===\n";
    world_->view<::Raiden::ECS::Name>().each(
        [&](::Raiden::ECS::Entity e, ::Raiden::ECS::Name &name) {
          ss << "  [" << e.index << "] " << name.value << "\n";
        });
    return ss.str();
  });

  addCommand("entity", [this](const std::string &args) {
    if (!world_) {
      return std::string("No world available");
    }
    uint32_t idx = 0;
    try {
      idx = static_cast<uint32_t>(std::stoul(args));
    } catch (...) {
      return std::string("Usage: entity <index>");
    }

    ::Raiden::ECS::Entity e{.index = idx, .generation = 0};
    std::ostringstream ss;
    ss << "=== Entity [" << idx << "] ===\n";

    world_->forEachComponent(e, [&](::Raiden::ECS::Entity,
                                    ::Raiden::ECS::ComponentId,
                                    std::string_view compName, void *data) {
      ss << "  " << compName << "\n";

      if (compName == "Name") {
        auto *name = static_cast<::Raiden::ECS::Name *>(data);
        ss << "    value: " << name->value << "\n";
      } else if (compName == "Transform") {
        auto *t = static_cast<::Raiden::ECS::Transform *>(data);
        ss << "    pos: (" << t->translation.x << ", " << t->translation.y
           << ", " << t->translation.z << ")\n";
        ss << "    scale: (" << t->scale.x << ", " << t->scale.y << ", "
           << t->scale.z << ")\n";
      } else if (compName == "MeshRenderer") {
        auto *mr = static_cast<::Raiden::ECS::MeshRenderer *>(data);
        ss << "    mesh: " << mr->meshPath << "\n";
        ss << "    texture: "
           << (mr->texturePath.empty() ? "(none)" : mr->texturePath) << "\n";
        ss << "    metallic: " << mr->metallic
           << "  roughness: " << mr->roughness << "\n";
      }
    });
    return ss.str();
  });
}

void DebugConsole::render(bool &open) {
  if (!open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Debug Console##DebugConsole", &open)) {
    ImGui::End();
    return;
  }

  // output region (scrollable)
  const float footerHeight =
      ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
  ImGui::BeginChild("ScrollRegion", ImVec2(0, -footerHeight),
                    ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto &line : outputLines_) {
    ImVec4 color = ImVec4(0.7F, 0.7F, 0.7F, 1.0F);
    if (line.find("[error]") != std::string::npos ||
        line.find("[warn]") != std::string::npos) {
      color = ImVec4(1.0F, 0.6F, 0.3F, 1.0F);
    } else if (line.find("===") != std::string::npos) {
      color = ImVec4(0.4F, 0.8F, 1.0F, 1.0F);
    } else if (line.find("->") == 0 || line.find(" = ") != std::string::npos) {
      color = ImVec4(0.5F, 1.0F, 0.5F, 1.0F);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(line.c_str());
    ImGui::PopStyleColor();
  }

  if (scrollToBottom_) {
    ImGui::SetScrollHereY(1.0F);
    scrollToBottom_ = false;
  }

  ImGui::EndChild();
  ImGui::Separator();

  // input line
  auto inputCallback = [](ImGuiInputTextCallbackData *data) -> int {
    auto *console = static_cast<DebugConsole *>(data->UserData);
    if (data->EventFlag & ImGuiInputTextFlags_CallbackHistory) {
      if (data->EventKey == ImGuiKey_UpArrow) {
        if (console->historyPos_ <
            static_cast<int>(console->history_.size()) - 1) {
          console->historyPos_++;
        }
      } else if (data->EventKey == ImGuiKey_DownArrow) {
        if (console->historyPos_ > -1) {
          console->historyPos_--;
        }
      }
      if (console->historyPos_ >= 0) {
        auto &cmd =
            console->history_[static_cast<size_t>(console->historyPos_)];
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, cmd.c_str());
      } else {
        data->DeleteChars(0, data->BufTextLen);
      }
    }
    if (data->EventFlag & ImGuiInputTextFlags_CallbackAlways) {
      console->inputFocused_ = ImGui::IsWindowFocused();
    }
    return 0;
  };

  bool reclaimFocus = false;
  ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                   ImGuiInputTextFlags_CallbackHistory |
                                   ImGuiInputTextFlags_CallbackAlways;

  if (ImGui::InputText("##ConsoleInput", inputBuf_, kMaxInputBuf, inputFlags,
                       inputCallback, this)) {
    std::string input(inputBuf_);
    if (!input.empty()) {
      executeCommand(input);
    }
    inputBuf_[0] = '\0';
    reclaimFocus = true;
  }

  ImGui::SameLine();
  ImGui::Text("(? for help)");

  if (reclaimFocus) {
    ImGui::SetKeyboardFocusHere(-1);
  }

  ImGui::End();
}

void DebugConsole::executeCommand(const std::string &input) {
  outputLines_.push_back("> " + input);

  if (input == "help" || input == "?") {
    auto it = commands_.find("help");
    if (it != commands_.end()) {
      auto result = it->second("");
      if (!result.empty()) {
        outputLines_.push_back(result);
      }
    }
    scrollToBottom_ = true;
    return;
  }

  // add to history
  if (history_.empty() || history_.back() != input) {
    history_.push_back(input);
    if (static_cast<int>(history_.size()) > kMaxHistory) {
      history_.erase(history_.begin());
    }
  }
  historyPos_ = -1;

  // parse command name and args
  auto space = input.find(' ');
  std::string cmdName;
  std::string cmdArgs;
  if (space != std::string::npos) {
    cmdName = input.substr(0, space);
    cmdArgs = input.substr(space + 1);
  } else {
    cmdName = input;
  }

  // check registered commands
  auto it = commands_.find(cmdName);
  if (it != commands_.end()) {
    auto result = it->second(cmdArgs);
    if (!result.empty()) {
      outputLines_.push_back(result);
    }
    scrollToBottom_ = true;
    return;
  }

  // try as ConVar set: "name value" or "name = value"
  auto &cv = Core::convars();
  if (cv.exists(cmdName)) {
    std::string value = cmdArgs;
    // strip leading "="
    if (!value.empty() && value[0] == '=') {
      value = value.substr(1);
      // trim leading space
      while (!value.empty() && value[0] == ' ') {
        value = value.substr(1);
      }
    }
    if (value.empty()) {
      // just print the value
      outputLines_.push_back(cmdName + " = " + cv.getString(cmdName, ""));
    } else {
      cv.setFromString(cmdName, value);
      outputLines_.push_back("-> " + cmdName + " = " +
                             cv.getString(cmdName, value));
    }
    scrollToBottom_ = true;
    return;
  }

  outputLines_.push_back("[error] Unknown command: " + cmdName);
  scrollToBottom_ = true;
}

void DebugConsole::addCommand(const std::string &name, CommandHandler handler) {
  commands_[name] = std::move(handler);
}

void DebugConsole::addOutputLine(const std::string &line) {
  outputLines_.push_back(line);
  if (static_cast<int>(outputLines_.size()) > kMaxOutputLines) {
    outputLines_.erase(outputLines_.begin());
  }
  scrollToBottom_ = true;
}

} // namespace Raiden::Engine
