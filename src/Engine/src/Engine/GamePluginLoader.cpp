#include <Raiden/Engine/GamePluginLoader.hpp>
#include <Raiden/Logger.hpp>

namespace Raiden::Engine {

static const ::Raiden::Core::Logger s_logger("Raiden::Engine::GamePluginLoader");

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

GamePluginLoader::~GamePluginLoader() { unload(); }

bool GamePluginLoader::load(std::string_view path) {
#ifdef _WIN32
  handle_ = reinterpret_cast<void *>(
      LoadLibraryA(path.data()));
  if (!handle_) {
    DWORD err = GetLastError();
    s_logger.error("Failed to load plugin '{}': error {}", path, err);
    return false;
  }

  auto *create = reinterpret_cast<IGamePlugin *(*)()>(
      GetProcAddress(reinterpret_cast<HMODULE>(handle_), "raiden_create_plugin"));
  destroy_ = reinterpret_cast<void (*)(IGamePlugin *)>(
      GetProcAddress(reinterpret_cast<HMODULE>(handle_), "raiden_destroy_plugin"));
#else

  std::string path_str(path);
  handle_ = dlopen(path_str.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle_ == nullptr) {
    s_logger.error("Failed to load plugin '{}': {}", path, dlerror());
    return false;
  }

  auto *create = reinterpret_cast<IGamePlugin *(*)()>(
      dlsym(handle_, "raiden_create_plugin"));
  destroy_ = reinterpret_cast<void (*)(IGamePlugin *)>(
      dlsym(handle_, "raiden_destroy_plugin"));
#endif

  if ((create == nullptr) || (destroy_ == nullptr)) {
    s_logger.error("Plugin '{}' missing create/destroy symbols", path);
    unload();
    return false;
  }

  plugin_ = create();
  if (plugin_ == nullptr) {
    s_logger.error("Plugin '{}' returned null from create", path);
    unload();
    return false;
  }

  s_logger.info("Loaded plugin '{}' ({})", plugin_->name(), path);
  return true;
}

void GamePluginLoader::registerPlugin(IGamePlugin *plugin) {
  plugin_ = plugin;
  destroy_ = nullptr;
  handle_ = nullptr;
}

void GamePluginLoader::unload() {
  if (plugin_ != nullptr) {
    plugin_->shutdown();
    if (destroy_ != nullptr) {
      destroy_(plugin_);
    } else {
      delete plugin_;
    }
    plugin_ = nullptr;
    destroy_ = nullptr;
  }
  
  if (handle_ != nullptr) {
#ifdef _WIN32
    FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
  }
}

} // namespace Raiden::Engine
