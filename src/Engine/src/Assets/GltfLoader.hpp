#pragma once

#include <Raiden/Renderer/Model.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace Raiden::Renderer { class IRenderDevice; }
namespace Raiden::Assets { class IAssetManager; }

namespace Raiden::Assets {

// loads a .glb/.gltf file from memory and produces engine Mesh objects
// textures are loaded through assets (which handles caching & format fallback)
// basePath is the VFS directory of the glTF file, used to resolve relative
// texture URIs
std::vector<::Raiden::Renderer::Mesh> loadGltf(::Raiden::Renderer::IRenderDevice &device, IAssetManager &assets,
                           const std::byte *data, size_t size,
                           std::string_view basePath);

} // namespace Raiden::Assets
