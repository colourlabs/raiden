#include <RaidenEngineCore/Engine/GamePluginLoader.hpp>
#include <RaidenEngineCore/Logger.hpp>

#include <dlfcn.h>

namespace Raiden::Core {

// TODO: this only works with Linux

static const Logger s_logger("Raiden::Core::GamePluginLoader");

GamePluginLoader::~GamePluginLoader() { unload(); }

bool GamePluginLoader::load(std::string_view path) {
  handle_ = dlopen(path.data(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    s_logger.error("Failed to load plugin '{}': {}", path, dlerror());
    return false;
  }

  auto *create = reinterpret_cast<IGamePlugin *(*)()>(
      dlsym(handle_, "raiden_create_plugin"));
  destroy_ = reinterpret_cast<void (*)(IGamePlugin *)>(
      dlsym(handle_, "raiden_destroy_plugin"));

  if (!create || !destroy_) {
    s_logger.error("Plugin '{}' missing create/destroy symbols", path);
    dlclose(handle_);
    handle_ = nullptr;
    return false;
  }

  plugin_ = create();
  if (!plugin_) {
    s_logger.error("Plugin '{}' returned null from create", path);
    dlclose(handle_);
    handle_ = nullptr;
    return false;
  }

  s_logger.info("Loaded plugin '{}' ({})", plugin_->name(), path);
  return true;
}

void GamePluginLoader::unload() {
  if (plugin_) {
    plugin_->shutdown();
    destroy_(plugin_);
    plugin_ = nullptr;
    destroy_ = nullptr;
  }
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
}

} // namespace Raiden::Core
