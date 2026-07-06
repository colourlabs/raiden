#pragma once

#include <RaidenUI/Render/FontAtlas.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Raiden::Core {
class IVirtualFileSystem;
}

namespace Raiden::Renderer {
class IRenderDevice;
}

namespace RaidenUI {

class FontFace {
public:
  FontFace(Raiden::Renderer::IRenderDevice &device,
           Raiden::Core::IVirtualFileSystem &vfs, std::string_view fontPath);

  FontAtlas &getAtlas(float fontSize);

private:
  Raiden::Renderer::IRenderDevice* device_;
  Raiden::Core::IVirtualFileSystem* vfs_;
  std::string fontPath_;
  std::unordered_map<float, std::unique_ptr<FontAtlas>> atlases_;
};

} // namespace RaidenUI
