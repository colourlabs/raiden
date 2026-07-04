#include <RaidenUI/Render/TextureCache.hpp>

namespace RaidenUI {

TextureCache::TextureCache(Raiden::Assets::IAssetManager &assetManager)
    : m_assetManager(assetManager) {}

std::shared_ptr<Raiden::Renderer::ITexture>
TextureCache::load(std::string_view path) {
  return m_assetManager.loadTexture(path);
}

std::shared_ptr<Raiden::Renderer::ITexture>
TextureCache::loadSync(std::string_view path) {
  return m_assetManager.loadTextureSync(path);
}

} // namespace RaidenUI
