#pragma once

#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <cstddef>
#include <memory>

namespace Raiden::Core {

class IRenderDevice;

// loads and transcodes a KTX2 file from memory
// returns a ready-to-use ITexture or nullptr on failure
std::shared_ptr<ITexture> loadKtx2(IRenderDevice &device, const std::byte *data,
                                   size_t size);

} // namespace Raiden::Core