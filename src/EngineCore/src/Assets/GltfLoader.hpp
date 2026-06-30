#pragma once

#include <RaidenEngineCore/Renderer/Model.hpp>

#include <cstddef>
#include <vector>

namespace Raiden::Core {

class IRenderDevice;

// loads a .glb/.gltf file from memory and produces engine Mesh objects
// returns empty vector on failure
std::vector<Mesh> loadGltf(IRenderDevice &device, const std::byte *data,
                           size_t size);

} // namespace Raiden::Core
