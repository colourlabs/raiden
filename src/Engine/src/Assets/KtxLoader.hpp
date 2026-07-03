#pragma once

#include "DecodedTextureData.hpp"
#include <cstddef>
#include <optional>

namespace Raiden::Assets {

// decodes and transcodes a KTX2 file from memory (no GPU upload)
// suitable for calling on worker threads
std::optional<DecodedTextureData> decodeKtx2(const std::byte *data,
                                             size_t size);

} // namespace Raiden::Assets
