#pragma once

#include <RaidenEngineCore/Engine/IGamePlugin.hpp>

extern "C" {

#ifdef _WIN32
#define RAIDEN_EXPORT __declspec(dllexport)
#else
#define RAIDEN_EXPORT __attribute__((visibility("default")))
#endif

RAIDEN_EXPORT Raiden::Core::IGamePlugin* raiden_create_plugin();
RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Core::IGamePlugin* plugin);

}
