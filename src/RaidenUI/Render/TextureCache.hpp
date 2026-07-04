#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Renderer/ITexture.hpp>

#include <memory>
#include <string>

namespace RaidenUI {

class TextureCache {
public:
  explicit TextureCache(Raiden::Assets::IAssetManager &assetManager);

  std::shared_ptr<Raiden::Renderer::ITexture> load(std::string_view path);
  std::shared_ptr<Raiden::Renderer::ITexture> loadSync(std::string_view path);

private:
  Raiden::Assets::IAssetManager &m_assetManager;
};

} // namespace RaidenUI
