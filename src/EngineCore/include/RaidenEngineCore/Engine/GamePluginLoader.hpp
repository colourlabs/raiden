#pragma once

#include <RaidenEngineCore/Engine/IGamePlugin.hpp>
#include <string_view>

namespace Raiden::Core {

class GamePluginLoader {
public:
  GamePluginLoader() = default;
  ~GamePluginLoader();

  GamePluginLoader(const GamePluginLoader &) = delete;
  GamePluginLoader &operator=(const GamePluginLoader &) = delete;

  bool load(std::string_view path);
  void unload();
  bool isLoaded() const { return plugin_ != nullptr; }

  IGamePlugin &plugin() { return *plugin_; }
  const IGamePlugin &plugin() const { return *plugin_; }

private:
  void *handle_ = nullptr;
  IGamePlugin *plugin_ = nullptr;
  void (*destroy_)(IGamePlugin *) = nullptr;
};

} // namespace Raiden::Core
