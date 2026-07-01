#pragma once

#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/IMaterial.hpp>
#include <RaidenEngineCore/Renderer/Model.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace Raiden::Core {

class IAssetManager {
public:
  virtual ~IAssetManager() = default;

  // register in-memory data at a virtual path (e.g. for embedded glTF textures)
  virtual void registerData(std::string_view path,
                            std::vector<std::byte> data) = 0;

  virtual std::shared_ptr<ITexture> loadTexture(std::string_view vfsPath) = 0;
  virtual std::shared_ptr<IMaterial> loadMaterial(const MaterialDesc &desc) = 0;
  virtual std::shared_ptr<Model> loadMesh(std::string_view vfsPath) = 0;

  // call on scene unload or explicitly, evicts anything with no live references
  virtual void releaseUnused() = 0;

  // call at start of frame, processes async loads that finished on worker
  // threads
  virtual void processLoadQueue() = 0;
};

}; // namespace Raiden::Core