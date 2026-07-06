#include <RaidenUI/Render/FontFace.hpp>

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>

#include <utility>

namespace RaidenUI {

FontFace::FontFace(Raiden::Renderer::IRenderDevice &device,
                   Raiden::Core::IVirtualFileSystem &vfs,
                   std::string_view fontPath)
    : device_(&device), vfs_(&vfs), fontPath_(fontPath) {}

FontAtlas &FontFace::getAtlas(float fontSize) {
  if (auto it = atlases_.find(fontSize); it != atlases_.end()) {
    return *it->second;
  }

  auto [it, inserted] = atlases_.try_emplace(
      fontSize,
      std::make_unique<FontAtlas>(*device_, *vfs_, fontPath_, fontSize));

  return *it->second;
}

} // namespace RaidenUI
