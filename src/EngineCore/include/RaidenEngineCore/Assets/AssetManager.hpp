#pragma once

#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/Core/IVirtualFileSystem.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace Raiden::Core {

class AssetManager final : public IAssetManager {
public:
  AssetManager(IRenderDevice &device, IVirtualFileSystem &vfs);
  ~AssetManager() override = default;

  void registerData(std::string_view path,
                    std::vector<std::byte> data) override;
  std::shared_ptr<ITexture> loadTexture(std::string_view vfsPath) override;
  std::shared_ptr<IMaterial> loadMaterial(const MaterialDesc &desc) override;
  std::shared_ptr<Model> loadMesh(std::string_view vfsPath) override;

  void releaseUnused() override;
  void processLoadQueue() override {}

  size_t cachedTextureCount() const { return textureCache_.size(); }
  size_t cachedMaterialCount() const { return materialCache_.size(); }
  size_t cachedMeshCount() const { return meshCache_.size(); }

private:
  IRenderDevice &device_;
  IVirtualFileSystem &vfs_;

  std::unordered_map<std::string, std::weak_ptr<ITexture>> textureCache_;
  std::unordered_map<std::string, std::weak_ptr<IMaterial>> materialCache_;
  std::unordered_map<std::string, std::weak_ptr<Model>> meshCache_;
};

} // namespace Raiden::Core