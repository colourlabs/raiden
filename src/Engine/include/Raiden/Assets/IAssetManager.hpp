#pragma once

#include <Raiden/Jobs/JobSystem.hpp>
#include <Raiden/Renderer/ITexture.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/Model.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace Raiden::Assets {

class IAssetManager {
public:
  virtual ~IAssetManager() = default;

  virtual void setJobSystem(::Raiden::Jobs::JobSystem &js) = 0;

  // register in-memory data at a virtual path (e.g. for embedded glTF textures)
  virtual void registerData(std::string_view path,
                            std::vector<std::byte> data) = 0;

  // async: returns nullptr while decode is pending on a worker thread
  virtual std::shared_ptr<::Raiden::Renderer::ITexture> loadTexture(std::string_view vfsPath) = 0;

  // synchronous: decode + GPU upload complete before returning
  virtual std::shared_ptr<::Raiden::Renderer::ITexture> loadTextureSync(std::string_view vfsPath) = 0;
  virtual std::shared_ptr<::Raiden::Renderer::IMaterial> loadMaterial(const ::Raiden::Renderer::MaterialDesc &desc) = 0;
  virtual std::shared_ptr<::Raiden::Renderer::Model> loadMesh(std::string_view vfsPath) = 0;

  // call on scene unload or explicitly, evicts anything with no live references
  virtual void releaseUnused() = 0;

  // call at start of frame, processes async loads that finished on worker
  // threads
  virtual void processLoadQueue() = 0;
};

}; // namespace Raiden::Assets
