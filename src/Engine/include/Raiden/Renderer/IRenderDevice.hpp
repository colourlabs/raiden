#pragma once

#include <Raiden/EngineConfig.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/ISampler.hpp>
#include <Raiden/Renderer/ITexture.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <functional>
#include <memory>

namespace Raiden::Platform { class IPlatform; }
namespace Raiden::Jobs { class JobSystem; }

namespace Raiden::Renderer {

class IRenderDevice {
public:
  IRenderDevice() = default;
  virtual ~IRenderDevice() = default;

  IRenderDevice(const IRenderDevice&) = delete;
  IRenderDevice& operator=(const IRenderDevice&) = delete;
  IRenderDevice(IRenderDevice&&) = delete;
  IRenderDevice& operator=(IRenderDevice&&) = delete;
  
  virtual bool init(const ::Raiden::Core::EngineConfig &config, ::Raiden::Platform::IPlatform *platform) = 0;
  virtual void shutdown() = 0;

  virtual std::unique_ptr<IBuffer> createBuffer(const BufferDesc &desc) = 0;
  virtual std::unique_ptr<IPipeline>
  createPipeline(const PipelineDesc &desc) = 0;
  virtual std::unique_ptr<ITexture> createTexture(const TextureDesc &desc) = 0;
  virtual std::unique_ptr<ISampler> createSampler(const SamplerDesc &desc) = 0;
  virtual std::shared_ptr<IMaterial>
  createMaterial(const MaterialDesc &desc,
                 std::shared_ptr<ITexture> albedo = nullptr,
                 std::shared_ptr<ITexture> normal = nullptr,
                 std::shared_ptr<ITexture> metallicRoughness = nullptr,
                 std::shared_ptr<ITexture> emissive = nullptr,
                 std::shared_ptr<ITexture> occlusion = nullptr) = 0;

  virtual void setJobSystem(::Raiden::Jobs::JobSystem &js) = 0;

  virtual void waitIdle() = 0;

  // workerIndex / totalWorkers let the callback split work across threads
  using RenderCallback =
      std::function<void(ICommandBuffer &, uint32_t workerIndex,
                         uint32_t totalWorkers)>;
  virtual bool drawFrame(const RenderCallback &callback) = 0;
};

} // namespace Raiden::Renderer
