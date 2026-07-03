#pragma once

#include "DecodedTextureData.hpp"
#include <cstddef>
#include <optional>

namespace Raiden::Assets {

// decodes PNG, JPEG, BMP, GIF, etc. via stb_image (no GPU upload)
// suitable for calling on worker threads
std::optional<DecodedTextureData> decodeStbImage(const std::byte *data,
                                                 size_t size);

} // namespace Raiden::Assets
