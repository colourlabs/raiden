#pragma once

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/EngineConfig.hpp>

#include <string>

namespace Raiden::Core {

bool loadConfig(IVirtualFileSystem &vfs, EngineConfig &outConfig,
                const std::string &defaultTitle = "raiden engine");

std::string resolvePluginPath(const PluginConfig &cfg);

} // namespace Raiden::Core
