#pragma once

#include <RaidenEngineCore/EngineConfig.hpp>
#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/IMaterial.hpp>
#include <RaidenEngineCore/Renderer/IPipeline.hpp>
#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#include <functional>
#include <memory>

namespace Raiden::Core {

class IPlatform;

class IRenderDevice {
public:
  virtual ~IRenderDevice() = default;
  virtual bool init(const EngineConfig &config, IPlatform *platform) = 0;
  virtual void shutdown() = 0;

  virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc &desc) = 0;
  virtual std::unique_ptr<IPipeline>
  createPipeline(const PipelineDesc &desc) = 0;
  virtual std::unique_ptr<ITexture> createTexture(const TextureDesc &desc) = 0;
  virtual std::shared_ptr<IMaterial>
  createMaterial(const MaterialDesc &desc,
                 std::shared_ptr<ITexture> albedo = nullptr,
                 std::shared_ptr<ITexture> normal = nullptr,
                 std::shared_ptr<ITexture> metallicRoughness = nullptr,
                 std::shared_ptr<ITexture> emissive = nullptr,
                 std::shared_ptr<ITexture> occlusion = nullptr) = 0;

  virtual void waitIdle() = 0;

  using RenderCallback = std::function<void(ICommandBuffer &)>;
  virtual bool drawFrame(const RenderCallback &callback) = 0;
};

} // namespace Raiden::Core