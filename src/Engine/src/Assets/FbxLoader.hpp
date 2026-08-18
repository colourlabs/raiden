#pragma once

#include <Raiden/Renderer/Model.hpp>

#include <cstddef>
#include <string_view>
#include <vector>

namespace Raiden::Renderer { class IRenderDevice; }
namespace Raiden::Assets { class IAssetManager; }

namespace Raiden::Assets {

std::vector<::Raiden::Renderer::Mesh> loadFbx(::Raiden::Renderer::IRenderDevice &device, IAssetManager &assets,
                           const std::byte *data, size_t size,
                           std::string_view basePath);

} // namespace Raiden::Assets
