#include <Raiden/Core/ConVar.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <charconv>
#include <sstream>

static const ::Raiden::Core::Logger s_logger("ConVar");

namespace Raiden::Core {

ConVarRegistry &ConVarRegistry::instance() {
  static ConVarRegistry s_instance;
  return s_instance;
}

void ConVarRegistry::registerInt(std::string name, int defaultValue,
                                 uint32_t flags, std::string description) {
  convars_[std::move(name)] = {
      .value = defaultValue,
      .defaultValue = defaultValue,
      .flags = flags,
      .description = std::move(description),
      .onChange = nullptr,
  };
}

void ConVarRegistry::registerFloat(std::string name, float defaultValue,
                                   uint32_t flags, std::string description) {
  convars_[std::move(name)] = {
      .value = defaultValue,
      .defaultValue = defaultValue,
      .flags = flags,
      .description = std::move(description),
      .onChange = nullptr,
  };
}

void ConVarRegistry::registerBool(std::string name, bool defaultValue,
                                  uint32_t flags, std::string description) {
  convars_[std::move(name)] = {
      .value = defaultValue,
      .defaultValue = defaultValue,
      .flags = flags,
      .description = std::move(description),
      .onChange = nullptr,
  };
}

void ConVarRegistry::registerString(std::string name, std::string defaultValue,
                                    uint32_t flags, std::string description) {
  convars_[std::move(name)] = {
      .value = std::move(defaultValue),
      .defaultValue = std::string(defaultValue),
      .flags = flags,
      .description = std::move(description),
      .onChange = nullptr,
  };
}

bool ConVarRegistry::setInt(std::string_view name, int value) {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return false;
  }

  if ((it->second.flags & ConVarReadOnly) != 0U) {
    s_logger.warn("ConVar '{}' is read-only.", name);
    return false;
  }
  
  it->second.value = value;
  if (it->second.onChange) {
    it->second.onChange(std::string(name), it->second.value);
  }
  
  return true;
}

bool ConVarRegistry::setFloat(std::string_view name, float value) {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return false;
  }

  if ((it->second.flags & ConVarReadOnly) != 0U) {
    s_logger.warn("ConVar '{}' is read-only.", name);
    return false;
  }

  it->second.value = value;
  if (it->second.onChange) {
    it->second.onChange(std::string(name), it->second.value);
  }
  return true;
}

bool ConVarRegistry::setBool(std::string_view name, bool value) {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return false;
  }
  
  if ((it->second.flags & ConVarReadOnly) != 0U) {
    s_logger.warn("ConVar '{}' is read-only.", name);
    return false;
  }

  it->second.value = value;
  if (it->second.onChange) {
    it->second.onChange(std::string(name), it->second.value);
  }
  
  return true;
}

bool ConVarRegistry::setString(std::string_view name, std::string value) {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return false;
  }
  if ((it->second.flags & ConVarReadOnly) != 0U) {
    s_logger.warn("ConVar '{}' is read-only.", name);
    return false;
  }
  it->second.value = std::move(value);
  if (it->second.onChange) {
    it->second.onChange(std::string(name), it->second.value);
  }
  return true;
}

static ConVarValue parseValue(std::string_view str) {
  // try bool first
  if (str == "true" || str == "1") {
    return true;
  }
  if (str == "false" || str == "0") {
    return false;
  }

  // try int
  {
    int iv = 0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), iv);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
      return iv;
    }
  }

  // try float
  {
    float fv = 0.0F;
    auto [ptr, ec] =
        std::from_chars(str.data(), str.data() + str.size(), fv);
    if (ec == std::errc() && ptr == str.data() + str.size()) {
      return fv;
    }
  }

  // fallback to string (strip quotes if present)
  std::string s(str);
  if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') ||
                         (s.front() == '\'' && s.back() == '\''))) {
    s = s.substr(1, s.size() - 2);
  }
  return s;
}

bool ConVarRegistry::setFromString(std::string_view name,
                                   std::string_view value) {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return false;
  }
  if ((it->second.flags & ConVarReadOnly) != 0U) {
    s_logger.warn("ConVar '{}' is read-only.", name);
    return false;
  }

  ConVarValue parsed = parseValue(value);

  // match type to existing convar type
  if (std::holds_alternative<int>(it->second.value)) {
    if (std::holds_alternative<int>(parsed)) {
      it->second.value = std::get<int>(parsed);
    } else if (std::holds_alternative<float>(parsed)) {
      it->second.value = static_cast<int>(std::get<float>(parsed));
    } else {
      s_logger.warn("ConVar '{}' expects int, got string.", name);
      return false;
    }
  } else if (std::holds_alternative<float>(it->second.value)) {
    if (std::holds_alternative<float>(parsed)) {
      it->second.value = std::get<float>(parsed);
    } else if (std::holds_alternative<int>(parsed)) {
      it->second.value = static_cast<float>(std::get<int>(parsed));
    } else {
      s_logger.warn("ConVar '{}' expects float, got string.", name);
      return false;
    }
  } else if (std::holds_alternative<bool>(it->second.value)) {
    if (std::holds_alternative<bool>(parsed)) {
      it->second.value = std::get<bool>(parsed);
    } else if (std::holds_alternative<int>(parsed)) {
      it->second.value = std::get<int>(parsed) != 0;
    } else {
      s_logger.warn("ConVar '{}' expects bool, got string.", name);
      return false;
    }
  } else if (std::holds_alternative<std::string>(it->second.value)) {
    if (std::holds_alternative<std::string>(parsed)) {
      it->second.value = std::move(parsed);
    } else if (std::holds_alternative<int>(parsed)) {
      it->second.value = std::to_string(std::get<int>(parsed));
    } else if (std::holds_alternative<float>(parsed)) {
      it->second.value = std::to_string(std::get<float>(parsed));
    } else if (std::holds_alternative<bool>(parsed)) {
      it->second.value = std::get<bool>(parsed) ? "true" : "false";
    }
  }

  if (it->second.onChange) {
    it->second.onChange(std::string(name), it->second.value);
  }
  return true;
}

int ConVarRegistry::getInt(std::string_view name, int fallback) const {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return fallback;
  }
  if (std::holds_alternative<int>(it->second.value)) {
    return std::get<int>(it->second.value);
  }
  return fallback;
}

float ConVarRegistry::getFloat(std::string_view name, float fallback) const {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return fallback;
  }
  if (std::holds_alternative<float>(it->second.value)) {
    return std::get<float>(it->second.value);
  }
  if (std::holds_alternative<int>(it->second.value)) {
    return static_cast<float>(std::get<int>(it->second.value));
  }
  return fallback;
}

bool ConVarRegistry::getBool(std::string_view name, bool fallback) const {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return fallback;
  }
  if (std::holds_alternative<bool>(it->second.value)) {
    return std::get<bool>(it->second.value);
  }
  if (std::holds_alternative<int>(it->second.value)) {
    return std::get<int>(it->second.value) != 0;
  }
  return fallback;
}

const std::string &ConVarRegistry::getString(std::string_view name,
                                             const std::string &fallback) const {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return fallback;
  }
  if (std::holds_alternative<std::string>(it->second.value)) {
    return std::get<std::string>(it->second.value);
  }
  return fallback;
}

bool ConVarRegistry::exists(std::string_view name) const {
  return convars_.find(std::string(name)) != convars_.end();
}

uint32_t ConVarRegistry::getFlags(std::string_view name) const {
  auto it = convars_.find(std::string(name));
  if (it == convars_.end()) {
    return ConVarNone;
  }
  return it->second.flags;
}

void ConVarRegistry::resetAll() {
  for (auto &[name, entry] : convars_) {
    entry.value = entry.defaultValue;
  }
}

bool ConVarRegistry::loadAutoExec(IVirtualFileSystem &vfs,
                                  std::string_view path) {
  if (!vfs.exists(path)) {
    s_logger.debug("No autoexec.cfg found at '{}'.", path);
    return false;
  }

  std::string content = vfs.readAll(path);
  if (content.empty()) {
    return false;
  }

  std::istringstream stream(content);
  std::string line;
  int lineNum = 0;

  while (std::getline(stream, line)) {
    lineNum++;

    // trim whitespace
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      continue;
    }
    line = line.substr(start);

    // skip comments
    if (line.front() == '/' && line.size() > 1 && line[1] == '/') {
      continue;
    }
    if (line.front() == '#') {
      continue;
    }

    // parse key = value
    auto eqPos = line.find('=');
    if (eqPos == std::string::npos) {
      s_logger.warn("autoexec.cfg:{}: invalid syntax: '{}'", lineNum, line);
      continue;
    }

    std::string key = line.substr(0, eqPos);
    std::string value = line.substr(eqPos + 1);

    // trim key
    auto keyEnd = key.find_last_not_of(" \t");
    if (keyEnd != std::string::npos) {
      key = key.substr(0, keyEnd + 1);
    }

    // trim value
    auto valStart = value.find_first_not_of(" \t");
    if (valStart == std::string::npos) {
      s_logger.warn("autoexec.cfg:{}: empty value for '{}'", lineNum, key);
      continue;
    }
    auto valEnd = value.find_last_not_of(" \t");
    value = value.substr(valStart, valEnd - valStart + 1);

    if (!setFromString(key, value)) {
      s_logger.warn("autoexec.cfg:{}: unknown convar '{}'", lineNum, key);
    }
  }

  s_logger.info("Loaded autoexec.cfg ({} lines).", lineNum);
  return true;
}

static std::string valueToString(const ConVarValue &val) {
  if (std::holds_alternative<int>(val)) {
    return std::to_string(std::get<int>(val));
  }
  if (std::holds_alternative<float>(val)) {
    return std::to_string(std::get<float>(val));
  }
  if (std::holds_alternative<bool>(val)) {
    return std::get<bool>(val) ? "true" : "false";
  }
  if (std::holds_alternative<std::string>(val)) {
    return std::get<std::string>(val);
  }
  return "";
}

bool ConVarRegistry::saveAutoExec(IVirtualFileSystem &vfs,
                                  std::string_view path) const {
  std::string content;
  content += "// auto-generated by raiden engine\n";
  content += "// do not edit manually\n\n";

  for (const auto &[name, entry] : convars_) {
    if ((entry.flags & ConVarHidden) != 0U) {
      continue;
    }

    if ((entry.flags & ConVarArchive) == 0U) {
      continue;
    }
    
    if (entry.description.empty()) {
      content += name + " = " + valueToString(entry.value) + "\n";
    } else {
      content += "// " + entry.description + "\n";
      content += name + " = " + valueToString(entry.value) + "\n";
    }
  }

  return vfs.write(path, content);
}

void ConVarRegistry::applyCLIOverrides(int argc, char *argv[]) {
  for (int i = 1; i < argc - 1; ++i) {
    std::string_view arg(argv[i]);
    if (arg.size() > 1 && arg.front() == '+') {
      std::string_view name = arg.substr(1);
      std::string_view value = argv[i + 1];
      if (!setFromString(name, value)) {
        s_logger.warn("CLI: unknown convar '{}'", name);
      } else {
        s_logger.debug("CLI: {} = {}", name, value);
      }
      i++; // skip the value
    }
  }
}

std::vector<ConVarRegistry::ConVarInfo> ConVarRegistry::getAll() const {
  std::vector<ConVarInfo> result;
  result.reserve(convars_.size());
  for (const auto &[name, entry] : convars_) {
    result.push_back({
        .name = name,
        .value = entry.value,
        .defaultValue = entry.defaultValue,
        .flags = entry.flags,
        .description = entry.description,
    });
  }
  return result;
}

void ConVarRegistry::setOnChange(std::string_view name,
                                 OnChangeCallback callback) {
  auto it = convars_.find(std::string(name));
  if (it != convars_.end()) {
    it->second.onChange = std::move(callback);
  }
}

} // namespace Raiden::Core
