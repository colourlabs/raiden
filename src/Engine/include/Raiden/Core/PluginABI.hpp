#pragma once

#include <Raiden/Engine/IGamePlugin.hpp>

extern "C" {

#ifdef _WIN32
#define RAIDEN_EXPORT __declspec(dllexport)
#else
#define RAIDEN_EXPORT __attribute__((visibility("default")))
#endif

RAIDEN_EXPORT Raiden::Engine::IGamePlugin* raiden_create_plugin();
RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Engine::IGamePlugin* plugin);

}
