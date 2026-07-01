#include "GltfLoader.hpp"
#include "KtxLoader.hpp"
#include "StbImageLoader.hpp"

#include <RaidenEngineCore/Assets/AssetManager.hpp>
#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::AssetManager");

AssetManager::AssetManager(IRenderDevice &device, IVirtualFileSystem &vfs)
    : device_(device), vfs_(vfs) {}

void AssetManager::registerData(std::string_view path,
                                std::vector<std::byte> data) {
  vfs_.registerData(path, std::move(data));
}

std::shared_ptr<ITexture> AssetManager::loadTexture(std::string_view vfsPath) {
  std::string key(vfsPath);

  auto it = textureCache_.find(key);
  if (it != textureCache_.end()) {
    if (auto live = it->second.lock())
      return live;
  }

  auto bytes = vfs_.readBytes(vfsPath);
  if (bytes.empty()) {
    s_logger.error("Texture file not found or empty: {}", vfsPath);
    return nullptr;
  }

  std::string path(vfsPath);
  bool isKtx2 = path.size() > 5 && path.substr(path.size() - 5) == ".ktx2";

  if (isKtx2) {
    auto tex = loadKtx2(device_, bytes.data(), bytes.size());
    if (!tex) {
      s_logger.error("Failed to load KTX2: {}", vfsPath);
      return nullptr;
    }
    textureCache_[key] = tex;
    return tex;
  }

  s_logger.warn(
      "Loading raw texture '{}' - consider using .ktx2 for optimal "
      "performance (smaller size, faster load and it's a GPU-ready format)!",
      vfsPath);

  auto tex = loadStbImage(device_, bytes.data(), bytes.size());
  if (!tex) {
    s_logger.error("Failed to load texture: {}", vfsPath);
    return nullptr;
  }
  textureCache_[key] = tex;
  return tex;
}

std::shared_ptr<IMaterial>
AssetManager::loadMaterial(const MaterialDesc &desc) {
  std::string key = desc.shader;
  
  auto append = [&](const auto &s) {
    if (!s.empty()) {
      key += '|';
      key += s;
    }
  };

  append(desc.baseColorTexture);
  append(desc.normalTexture);
  append(desc.metallicRoughnessTexture);
  append(desc.emissiveTexture);
  append(desc.occlusionTexture);

  auto it = materialCache_.find(key);
  if (it != materialCache_.end()) {
    if (auto live = it->second.lock())
      return live;
  }

  std::shared_ptr<ITexture> albedo;
  std::shared_ptr<ITexture> normal;
  std::shared_ptr<ITexture> metallicRoughness;
  std::shared_ptr<ITexture> emissive;
  std::shared_ptr<ITexture> occlusion;

  if (!desc.baseColorTexture.empty())
    albedo = loadTexture(desc.baseColorTexture);
  if (!desc.normalTexture.empty())
    normal = loadTexture(desc.normalTexture);
  if (!desc.metallicRoughnessTexture.empty())
    metallicRoughness = loadTexture(desc.metallicRoughnessTexture);
  if (!desc.emissiveTexture.empty())
    emissive = loadTexture(desc.emissiveTexture);
  if (!desc.occlusionTexture.empty())
    occlusion = loadTexture(desc.occlusionTexture);

  // device handles all backend-specific details internally
  auto mat = device_.createMaterial(desc, albedo, normal, metallicRoughness,
                                    emissive, occlusion);
  if (!mat) {
    s_logger.error("Failed to create material: {}", key);
    return nullptr;
  }

  materialCache_[key] = mat;
  s_logger.info("Material created: {}", key);
  return mat;
}

std::shared_ptr<Model> AssetManager::loadMesh(std::string_view vfsPath) {
  std::string key(vfsPath);

  auto it = meshCache_.find(key);
  if (it != meshCache_.end()) {
    if (auto live = it->second.lock())
      return live;
  }

  auto bytes = vfs_.readBytes(vfsPath);
  if (bytes.empty()) {
    s_logger.error("Mesh file not found or empty: {}", vfsPath);
    return nullptr;
  }

  std::string path(vfsPath);
  bool isGlb = path.size() > 4 && path.substr(path.size() - 4) == ".glb";
  bool isGltf = path.size() > 5 && path.substr(path.size() - 5) == ".gltf";

  if (!isGlb && !isGltf) {
    s_logger.warn("Unsupported mesh format: {}", vfsPath);
    return nullptr;
  }

  auto meshes = loadGltf(device_, *this, bytes.data(), bytes.size(), vfsPath);
  if (meshes.empty()) {
    s_logger.error("Failed to load glTF: {}", vfsPath);
    return nullptr;
  }

  auto model = std::make_shared<Model>();
  model->meshes = std::move(meshes);
  meshCache_[key] = model;
  s_logger.info("glTF loaded: {} meshes from {}", model->meshes.size(),
                vfsPath);
  return model;
}

void AssetManager::releaseUnused() {
  std::erase_if(textureCache_, [](auto &kv) { return kv.second.expired(); });
  std::erase_if(materialCache_, [](auto &kv) { return kv.second.expired(); });
  std::erase_if(meshCache_, [](auto &kv) { return kv.second.expired(); });
}

} // namespace Raiden::Core