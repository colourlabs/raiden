#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace RaidenEditor {

struct ProjectInfo {
  std::string name;
  std::string path;
  std::string pluginPath;
  std::filesystem::file_time_type lastModified{};
  bool hasPlugin = false;
};

class ProjectDiscovery {
public:
  static std::vector<ProjectInfo> scanDirectory(const std::string &dirPath);
  static std::string defaultGamesDir();
  static std::string recentProjectsPath();
  static std::vector<std::string> loadRecent();
  static void saveRecent(const std::string &projectPath);
};

} // namespace RaidenEditor
