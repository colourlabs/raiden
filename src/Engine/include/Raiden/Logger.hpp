#pragma once

#include <array>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Raiden::Core {

inline void enableWindowsAnsi() {
#ifdef _WIN32
  auto enable = [](DWORD handleId) {
    HANDLE h = GetStdHandle(handleId);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return;

    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  };

  enable(STD_OUTPUT_HANDLE);
  enable(STD_ERROR_HANDLE);
#endif
}

enum class LogLevel : std::uint8_t { Debug, Info, Warning, Error, Critical };

class Logger {
public:
  explicit Logger(std::string_view tag) : tag_(tag) {}

  template <typename... Args>
  void debug(std::string_view fmt, Args &&...args) const {
    if (should_log(LogLevel::Debug)) {
      log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void info(std::string_view fmt, Args &&...args) const {
    if (should_log(LogLevel::Info)) {
      log(LogLevel::Info, fmt, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void warn(std::string_view fmt, Args &&...args) const {
    if (should_log(LogLevel::Warning)) {
      log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void error(std::string_view fmt, Args &&...args) const {
    if (should_log(LogLevel::Error)) {
      log(LogLevel::Error, fmt, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  void critical(std::string_view fmt, Args &&...args) const {
    if (should_log(LogLevel::Critical)) {
      log(LogLevel::Critical, fmt, std::forward<Args>(args)...);
    }
  }

  template <typename... Args>
  static void log_static(LogLevel level, std::string_view tag,
                         std::string_view fmt, Args &&...args) {
    if (should_log(level)) {
      if constexpr (sizeof...(args) == 0) {
        log_internal(level, tag, fmt);
      } else {
        log_internal(level, tag,
                     std::vformat(fmt, std::make_format_args(args...)));
      }
    }
  }

  static void setMinLogLevel(LogLevel level);
  static LogLevel getMinLogLevel();

  using LogSink = std::function<void(LogLevel, std::string_view, std::string_view, std::string_view)>;
  static void setLogSink(LogSink sink);

private:
  template <typename... Args>
  void log(LogLevel level, std::string_view fmt, Args &&...args) const {
    if constexpr (sizeof...(args) == 0) {
      log_internal(level, tag_, fmt);
    } else {
      log_internal(level, tag_,
                   std::vformat(fmt, std::make_format_args(args...)));
    }
  }

  static bool should_log(LogLevel level);
  static void log_internal(LogLevel level, std::string_view tag,
                           std::string_view message);

  std::string tag_;
};

} // namespace Raiden::Core

// global convenience macros
#define RAIDEN_LOG_DEBUG(tag, ...)                                             \
  ::Raiden::Core::Logger::log_static(::Raiden::Core::LogLevel::Debug, tag,     \
                                     __VA_ARGS__)
#define RAIDEN_LOG_INFO(tag, ...)                                              \
  ::Raiden::Core::Logger::log_static(::Raiden::Core::LogLevel::Info, tag,      \
                                     __VA_ARGS__)
#define RAIDEN_LOG_WARN(tag, ...)                                              \
  ::Raiden::Core::Logger::log_static(::Raiden::Core::LogLevel::Warning, tag,   \
                                     __VA_ARGS__)
#define RAIDEN_LOG_ERROR(tag, ...)                                             \
  ::Raiden::Core::Logger::log_static(::Raiden::Core::LogLevel::Error, tag,     \
                                     __VA_ARGS__)
#define RAIDEN_LOG_CRITICAL(tag, ...)                                          \
  ::Raiden::Core::Logger::log_static(::Raiden::Core::LogLevel::Critical, tag,  \
                                     __VA_ARGS__)
