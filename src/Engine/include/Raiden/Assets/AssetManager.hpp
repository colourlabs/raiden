#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Jobs/JobSystem.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Raiden::Assets {

class AssetManager final : public IAssetManager {
public:
  AssetManager(::Raiden::Renderer::IRenderDevice &device, ::Raiden::Core::IVirtualFileSystem &vfs);
  ~AssetManager() override = default;

  void setJobSystem(::Raiden::Jobs::JobSystem &js) override { js_ = &js; }

  void registerData(std::string_view path,
                    std::vector<std::byte> data) override;
  std::shared_ptr<::Raiden::Renderer::ITexture> loadTexture(std::string_view vfsPath) override;
  std::shared_ptr<::Raiden::Renderer::ITexture> loadTextureSync(std::string_view vfsPath) override;
  std::shared_ptr<::Raiden::Renderer::IMaterial> loadMaterial(const ::Raiden::Renderer::MaterialDesc &desc) override;
  std::shared_ptr<::Raiden::Renderer::Model> loadMesh(std::string_view vfsPath) override;

  void releaseUnused() override;
  void processLoadQueue() override;

  size_t cachedTextureCount() const { return textureCache_.size(); }
  size_t cachedMaterialCount() const { return materialCache_.size(); }
  size_t cachedMeshCount() const { return meshCache_.size(); }

private:
  std::shared_ptr<::Raiden::Renderer::ITexture> _loadTextureSync(std::string_view vfsPath);

  struct PendingTexture {
    std::string key;
    std::vector<std::byte> pixels;
    int width = 0;
    int height = 0;
    ::Raiden::Renderer::Format format = ::Raiden::Renderer::Format::R8G8B8A8_SRGB;
    ::Raiden::Renderer::TextureType type = ::Raiden::Renderer::TextureType::Texture2D;
    bool failed = false;
    ::Raiden::Jobs::JobCounter counter;
  };
  
  std::vector<std::unique_ptr<PendingTexture>> pendingTextures_;

  ::Raiden::Renderer::IRenderDevice &device_;
  ::Raiden::Core::IVirtualFileSystem &vfs_;
  ::Raiden::Jobs::JobSystem *js_ = nullptr;

  std::unordered_map<std::string, std::weak_ptr<::Raiden::Renderer::ITexture>> textureCache_;
  std::unordered_map<std::string, std::weak_ptr<::Raiden::Renderer::IMaterial>> materialCache_;
  std::unordered_map<std::string, std::weak_ptr<::Raiden::Renderer::Model>> meshCache_;
};

} // namespace Raiden::Assets
