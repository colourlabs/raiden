#include <RaidenEngineCore/Engine/GamePluginLoader.hpp>
#include <RaidenEngineCore/Logger.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::GamePluginLoader");

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

GamePluginLoader::~GamePluginLoader() { unload(); }

bool GamePluginLoader::load(std::string_view path) {
#if defined(_WIN32)
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

  handle_ = dlopen(path.data(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    s_logger.error("Failed to load plugin '{}': {}", path, dlerror());
    return false;
  }

  auto *create = reinterpret_cast<IGamePlugin *(*)()>(
      dlsym(handle_, "raiden_create_plugin"));
  destroy_ = reinterpret_cast<void (*)(IGamePlugin *)>(
      dlsym(handle_, "raiden_destroy_plugin"));
#endif

  if (!create || !destroy_) {
    s_logger.error("Plugin '{}' missing create/destroy symbols", path);
    unload();
    return false;
  }

  plugin_ = create();
  if (!plugin_) {
    s_logger.error("Plugin '{}' returned null from create", path);
    unload();
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
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_ = nullptr;
  }
}

} // namespace Raiden::Core
