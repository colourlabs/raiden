#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace Raiden::Core {

enum class LogLevel { Debug, Info, Warning, Error, Critical };

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
