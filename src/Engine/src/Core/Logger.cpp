#include <Raiden/Logger.hpp>

#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>

namespace Raiden::Core {

namespace {

LogLevel g_minLogLevel = LogLevel::Info; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::mutex g_logMutex; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char *get_level_color(LogLevel level) {
  switch (level) {
  case LogLevel::Debug:
    return "\033[90m"; // bright gray
  case LogLevel::Info:
    return "\033[32m"; // green
  case LogLevel::Warning:
    return "\033[33m"; // yellow
  case LogLevel::Error:
    return "\033[31m"; // red
  case LogLevel::Critical:
    return "\033[1;31m"; // bold red
  }
  return "\033[0m";
}

const char *get_level_string(LogLevel level) {
  switch (level) {
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warning:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Critical:
    return "CRITICAL";
  }
  return "UNKNOWN";
}

std::string get_current_time_string() {
  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::tm local_tm;
#if defined(_MSC_VER)
  localtime_s(&local_tm, &time_t_now);
#else
  localtime_r(&time_t_now, &local_tm);
#endif

  std::array<char, 10> time_buffer; // HH:MM:SS\0
  std::strftime(time_buffer.data(), time_buffer.size(), "%H:%M:%S", &local_tm);

  return std::format("{}.{:03}", std::string(time_buffer.data()), ms.count());
}

} // namespace

void Logger::setMinLogLevel(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  g_minLogLevel = level;
}

LogLevel Logger::getMinLogLevel() {
  std::lock_guard<std::mutex> lock(g_logMutex);
  return g_minLogLevel;
}

bool Logger::should_log(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_logMutex);
  return level >= g_minLogLevel;
}

void Logger::log_internal(LogLevel level, std::string_view tag,
                          std::string_view message) {
  static bool ansiInitialized = (enableWindowsAnsi(), true);
  std::lock_guard<std::mutex> lock(g_logMutex);

  std::string timeStr = get_current_time_string();

  std::string logLine = std::format(
      "[\033[36m{}\033[0m] [\033[90m{}\033[0m] [{}{}\033[0m] {}\n", tag,
      timeStr, get_level_color(level), get_level_string(level), message);

  if (level >= LogLevel::Error) {
    std::cerr << logLine << std::flush;
  } else {
    std::cout << logLine << std::flush;
  }
}

} // namespace Raiden::Core
