#include "DecodedTextureData.hpp"
#include "GltfLoader.hpp"
#include "KtxLoader.hpp"
#include "StbImageLoader.hpp"

#include <Raiden/Assets/AssetManager.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

namespace Raiden::Assets {

using namespace ::Raiden::Core;
using namespace ::Raiden::Renderer;

static const ::Raiden::Core::Logger s_logger("Raiden::Assets::AssetManager");

AssetManager::AssetManager(IRenderDevice &device, IVirtualFileSystem &vfs)
    : device_(device), vfs_(vfs) {}

void AssetManager::registerData(std::string_view path,
                                std::vector<std::byte> data) {
  vfs_.registerData(path, std::move(data));
}

std::shared_ptr<ITexture> AssetManager::loadTextureSync(
    std::string_view vfsPath) {
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

  auto decoded = isKtx2 ? decodeKtx2(bytes.data(), bytes.size())
                        : decodeStbImage(bytes.data(), bytes.size());

  if (!decoded) {
    s_logger.error("Failed to decode texture: {}", vfsPath);
    return nullptr;
  }

  if (!isKtx2) {
    s_logger.warn(
        "Loading raw texture '{}' - consider using .ktx2 for optimal "
        "performance (smaller size, faster load and it's a GPU-ready format)!",
        vfsPath);
  }

  TextureDesc desc{
      .width = static_cast<uint32_t>(decoded->width),
      .height = static_cast<uint32_t>(decoded->height),
      .format = decoded->format,
      .type = decoded->type,
  };

  auto tex = device_.createTexture(desc);
  if (!tex) {
    s_logger.error("Failed to create GPU texture for: {}", vfsPath);
    return nullptr;
  }

  tex->upload(decoded->pixels.data(), decoded->pixels.size());
  auto texShared = std::shared_ptr<ITexture>(std::move(tex));
  textureCache_[key] = texShared;
  return texShared;
}

std::shared_ptr<ITexture> AssetManager::_loadTextureSync(
    std::string_view vfsPath) {
  return loadTextureSync(vfsPath);
}

// async texture load (decode on worker, upload deferred to processLoadQueue)
std::shared_ptr<ITexture> AssetManager::loadTexture(std::string_view vfsPath) {
  std::string key(vfsPath);

  // check cache
  auto it = textureCache_.find(key);
  if (it != textureCache_.end()) {
    if (auto live = it->second.lock())
      return live;
  }

  // check if already pending
  for (auto &p : pendingTextures_) {
    if (p->key == key)
      return nullptr;
  }

  if (!js_) {
    s_logger.warn("No JobSystem set, falling back to synchronous load: {}",
                  vfsPath);
    return _loadTextureSync(vfsPath);
  }

  auto pending = std::make_unique<PendingTexture>();
  pending->key = key;

  auto *rawPtr = pending.get();

  auto ctr = js_->submit(
      {.task = [this, rawPtr, path = std::string(vfsPath)]() {
        auto bytes = vfs_.readBytes(path);
        if (bytes.empty()) {
          rawPtr->failed = true;
          return;
        }

        bool isKtx2 =
            path.size() > 5 && path.substr(path.size() - 5) == ".ktx2";
        auto decoded = isKtx2
                           ? decodeKtx2(bytes.data(), bytes.size())
                           : decodeStbImage(bytes.data(), bytes.size());

        if (decoded) {
          rawPtr->width = decoded->width;
          rawPtr->height = decoded->height;
          rawPtr->format = decoded->format;
          rawPtr->type = decoded->type;
          rawPtr->pixels = std::move(decoded->pixels);
        } else {
          rawPtr->failed = true;
        }
      }});

  pending->counter = std::move(ctr);
  pendingTextures_.push_back(std::move(pending));
  return nullptr;
}

// processLoadQueue, called each frame on the main thread
void AssetManager::processLoadQueue() {
  for (auto it = pendingTextures_.begin(); it != pendingTextures_.end();) {
    auto &p = *it;
    if (!p->counter.isComplete()) {
      ++it;
      continue;
    }

    if (p->failed) {
      s_logger.error("Async texture load failed: {}", p->key);
      it = pendingTextures_.erase(it);
      continue;
    }

    // decode finished, create GPU texture and upload on main thread
    TextureDesc desc{
        .width = static_cast<uint32_t>(p->width),
        .height = static_cast<uint32_t>(p->height),
        .format = p->format,
        .type = p->type,
    };

    auto tex = device_.createTexture(desc);
    if (tex) {
      tex->upload(p->pixels.data(), p->pixels.size());
      textureCache_[p->key] = std::shared_ptr<ITexture>(std::move(tex));
      s_logger.info("Async texture loaded: {} ({}x{})", p->key, p->width,
                    p->height);
    } else {
      s_logger.error("Failed to create GPU texture for async load: {}",
                     p->key);
    }

    it = pendingTextures_.erase(it);
  }
}

// material loading (internally uses synchronous texture loads)
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
    albedo = _loadTextureSync(desc.baseColorTexture);
  if (!desc.normalTexture.empty())
    normal = _loadTextureSync(desc.normalTexture);
  if (!desc.metallicRoughnessTexture.empty())
    metallicRoughness = _loadTextureSync(desc.metallicRoughnessTexture);
  if (!desc.emissiveTexture.empty())
    emissive = _loadTextureSync(desc.emissiveTexture);
  if (!desc.occlusionTexture.empty())
    occlusion = _loadTextureSync(desc.occlusionTexture);

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

// mesh loading (synchronous, glTF parse + texture/material loads)
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

} // namespace Raiden::Assets
