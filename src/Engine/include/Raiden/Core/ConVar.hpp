#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Raiden::Core {

class IVirtualFileSystem;

enum ConVarFlags : uint8_t {
  ConVarNone = 0,
  ConVarArchive = 1 << 0,  // persist to autoexec.cfg on shutdown
  ConVarRestart = 1 << 1,  // requires restart to take effect
  ConVarCheat = 1 << 2,    // can't be changed in production builds
  ConVarReadOnly = 1 << 3, // can't be changed at runtime
  ConVarHidden = 1 << 4,   // don't show in debug UI
};

using ConVarValue = std::variant<int, float, bool, std::string>;

class ConVarRegistry {
public:
  static ConVarRegistry &instance();

  // register convars with defaults
  void registerInt(std::string name, int defaultValue,
                   uint32_t flags = ConVarNone,
                   std::string description = "");
  void registerFloat(std::string name, float defaultValue,
                     uint32_t flags = ConVarNone,
                     std::string description = "");
  void registerBool(std::string name, bool defaultValue,
                    uint32_t flags = ConVarNone,
                    std::string description = "");
  void registerString(std::string name, std::string defaultValue,
                      uint32_t flags = ConVarNone,
                      std::string description = "");

  // set values (returns false if convar doesn't exist or is read-only)
  bool setInt(std::string_view name, int value);
  bool setFloat(std::string_view name, float value);
  bool setBool(std::string_view name, bool value);
  bool setString(std::string_view name, std::string value);
  bool setFromString(std::string_view name, std::string_view value);

  // get values (returns fallback if convar doesn't exist)
  [[nodiscard]] int getInt(std::string_view name, int fallback = 0) const;
  [[nodiscard]] float getFloat(std::string_view name,
                               float fallback = 0.0F) const;
  [[nodiscard]] bool getBool(std::string_view name,
                             bool fallback = false) const;
  [[nodiscard]] const std::string &getString(
      std::string_view name,
      const std::string &fallback = "") const;

  // check existence
  [[nodiscard]] bool exists(std::string_view name) const;
  [[nodiscard]] uint32_t getFlags(std::string_view name) const;

  // reset all convars to defaults
  void resetAll();

  // load/save from autoexec.cfg (key = value format)
  bool loadAutoExec(IVirtualFileSystem &vfs,
                    std::string_view path = "game://autoexec.cfg");
  bool saveAutoExec(IVirtualFileSystem &vfs,
                    std::string_view path = "game://autoexec.cfg") const;

  // apply CLI overrides (+convarname value)
  void applyCLIOverrides(int argc, char *argv[]); // NOLINT

  // debug UI / introspection
  struct ConVarInfo {
    std::string name;
    ConVarValue value;
    ConVarValue defaultValue;
    uint32_t flags;
    std::string description;
  };

  [[nodiscard]] std::vector<ConVarInfo> getAll() const;

  // set change callback (called after value changes)
  using OnChangeCallback = std::function<void(const std::string &name,
                                              const ConVarValue &newValue)>;
  void setOnChange(std::string_view name, OnChangeCallback callback);

private:
  struct ConVarEntry {
    ConVarValue value;
    ConVarValue defaultValue;
    uint32_t flags;
    std::string description;
    OnChangeCallback onChange;
  };

  std::unordered_map<std::string, ConVarEntry> convars_;

  const std::string emptyString_; // NOLINT
};

// global convenience accessor
[[nodiscard]] inline ConVarRegistry &convars() {
  return ConVarRegistry::instance();
}

} // namespace Raiden::Core
