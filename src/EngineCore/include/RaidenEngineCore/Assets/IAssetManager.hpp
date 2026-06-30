#pragma once

#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/IMaterial.hpp>
#include <RaidenEngineCore/Renderer/Model.hpp>

#include <memory>

namespace Raiden::Core {

class IAssetManager {
public:
  virtual ~IAssetManager() = default;

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