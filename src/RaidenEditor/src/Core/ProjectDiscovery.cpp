#include <RaidenEditor/Core/ProjectDiscovery.hpp>

#include <Raiden/Logger.hpp>

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>

static const Raiden::Core::Logger s_logger("ProjectDiscovery");

namespace {

std::string getConfigDir() {
  const char *home = nullptr;
#ifdef _WIN32
  home = std::getenv("USERPROFILE");
#else
  home = std::getenv("HOME");
#endif
  if (home == nullptr) {
    return {};
  }

  std::string dir;
#ifdef _WIN32
  dir = std::string(home) + "/.raiden";
#else
  dir = std::string(home) + "/.config/raiden";
#endif

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

} // namespace

namespace RaidenEditor {

std::vector<ProjectInfo>
ProjectDiscovery::scanDirectory(const std::string &dirPath) {
  std::vector<ProjectInfo> projects;

  std::error_code ec;
  if (!std::filesystem::is_directory(dirPath, ec)) {
    s_logger.warn("Games directory not found: '{}'", dirPath);
    return projects;
  }

  for (const auto &entry :
       std::filesystem::directory_iterator(dirPath, ec)) {
    if (!entry.is_directory()) {
      continue;
    }

    auto tomlPath = entry.path() / "engine.toml";
    if (!std::filesystem::exists(tomlPath)) {
      continue;
    }

    ProjectInfo info;
    info.path = entry.path().string();

    try {
      auto table = toml::parse_file(tomlPath.string());

      // window title as project name
      if (auto *win = table["window"].as_table()) {
        info.name = (*win)["title"].value_or(std::string("Untitled"));
      } else {
        info.name = entry.path().filename().string();
      }

      // plugin path
      if (auto *platform = table["game.platform"].as_table()) {
#ifdef _WIN32
        info.pluginPath = (*platform)["windows"].value_or(std::string());
#elif defined(__APPLE__)
        auto arm =
            (*platform)["macosArm"].value_or(std::string());
        auto x86 =
            (*platform)["macosX86"].value_or(std::string());
        info.pluginPath = !arm.empty() ? arm : x86;
#else
        info.pluginPath = (*platform)["linux"].value_or(std::string());
#endif
        info.hasPlugin = !info.pluginPath.empty();
      }
    } catch (const toml::parse_error &err) {
      s_logger.warn("Failed to parse '{}/engine.toml': {}", info.path,
                     err.description());
      info.name = entry.path().filename().string();
    }

    // last modified time
    std::error_code sec;
    info.lastModified = last_write_time(entry.path(), sec);

    projects.push_back(std::move(info));
  }

  // sort by last modified (newest first)
  std::sort(projects.begin(), projects.end(),
            [](const ProjectInfo &a, const ProjectInfo &b) {
              return a.lastModified > b.lastModified;
            });

  s_logger.info("Found {} project(s) in '{}'", projects.size(), dirPath);
  return projects;
}

std::string ProjectDiscovery::defaultGamesDir() {
  // look for games/ relative to the editor executable
  auto exePath = std::filesystem::current_path();

  // try common build layout: build/editor/RaidenEditor -> build/games/
  auto candidate = exePath / ".." / ".." / "games";
  if (std::filesystem::is_directory(candidate)) {
    return std::filesystem::absolute(candidate).string();
  }

  // try games/ in current directory
  candidate = exePath / "games";
  if (std::filesystem::is_directory(candidate)) {
    return candidate.string();
  }

  // fallback: just use "games"
  return (exePath / "games").string();
}

std::string ProjectDiscovery::recentProjectsPath() {
  auto dir = getConfigDir();
  if (dir.empty()) {
    return {};
  }
  return dir + "/recent_projects.json";
}

std::vector<std::string> ProjectDiscovery::loadRecent() {
  auto path = recentProjectsPath();
  if (path.empty()) {
    return {};
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return {};
  }

  try {
    nlohmann::json j = nlohmann::json::parse(file);
    if (j.is_array()) {
      std::vector<std::string> result;
      for (const auto &item : j) {
        if (item.is_string()) {
          result.push_back(item.get<std::string>());
        }
      }
      return result;
    }
  } catch (...) {
    s_logger.warn("Failed to parse recent projects file");
  }

  return {};
}

void ProjectDiscovery::saveRecent(const std::string &projectPath) {
  auto path = recentProjectsPath();
  if (path.empty()) {
    return;
  }

  auto recent = loadRecent();

  // remove if already exists, then push to front
  recent.erase(
      std::remove(recent.begin(), recent.end(), projectPath), recent.end());
  recent.insert(recent.begin(), projectPath);

  // cap at 10
  if (recent.size() > 10) {
    recent.resize(10);
  }

  nlohmann::json j = recent;
  std::ofstream file(path);
  if (file.is_open()) {
    file << j.dump(2) << "\n";
    s_logger.info("Updated recent projects: '{}'", path);
  }
}

} // namespace RaidenEditor
