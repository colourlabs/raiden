#pragma once

#include <Raiden/Core/ConVar.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Raiden::ECS {
class World;
} // namespace Raiden::ECS

namespace Raiden::Engine {

class DebugConsole {
public:
  using CommandHandler = std::function<std::string(const std::string &args)>;

  DebugConsole() = default;

  void init();
  void render(bool &open);
  void setWorld(::Raiden::ECS::World *w) { world_ = w; }

  void addCommand(const std::string &name, CommandHandler handler);
  void addOutputLine(const std::string &line);
  bool wantsCaptureKeyboard() const { return inputFocused_; }

private:
  void executeCommand(const std::string &input);

  static constexpr int kMaxHistory = 64;
  static constexpr int kMaxOutputLines = 256;
  static constexpr int kMaxInputBuf = 512;

  char inputBuf_[kMaxInputBuf] = {};
  int historyPos_ = -1;
  std::vector<std::string> history_;
  std::vector<std::string> outputLines_;

  std::unordered_map<std::string, CommandHandler> commands_;
  ::Raiden::ECS::World *world_ = nullptr;

  bool inputFocused_ = false;
  bool scrollToBottom_ = false;
};

} // namespace Raiden::Engine
