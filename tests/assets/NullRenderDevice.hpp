#pragma once

#include <Raiden/EngineConfig.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>

#include <cstdint>

namespace Raiden::Renderer {

class NullTexture final : public ITexture {
public:
  explicit NullTexture(TextureDesc desc) : desc_(desc) {}
  void upload(const void * /*data*/, size_t /*size*/) override {}
  [[nodiscard]] uint32_t getWidth() const override { return desc_.width; }
  [[nodiscard]] uint32_t getHeight() const override { return desc_.height; }
  [[nodiscard]] Format getFormat() const override { return desc_.format; }
  [[nodiscard]] TextureType getType() const override { return desc_.type; }
  [[nodiscard]] uint32_t getMipLevels() const override { return 1; }

  TextureDesc desc_;
};

class NullMaterial final : public IMaterial {
public:
  explicit NullMaterial(MaterialDesc desc) : desc_(std::move(desc)) {}
  void bind(ICommandBuffer & /*cmd*/) override {}
  MaterialDesc desc_;
};

class NullRenderDevice final : public IRenderDevice {
public:
  bool init(const ::Raiden::Core::EngineConfig & /*config*/, ::Raiden::Platform::IPlatform * /*platform*/) override { return true; }
  void shutdown() override {}
  
  std::unique_ptr<IBuffer> createBuffer(const BufferDesc & /*desc*/) override {
    return nullptr;
  }
  
  std::unique_ptr<IPipeline> createPipeline(const PipelineDesc & /*desc*/) override {
    return nullptr;
  }
  
  std::unique_ptr<ITexture> createTexture(const TextureDesc &desc) override {
    createCount_++;
    return std::make_unique<NullTexture>(desc);
  }
  
  std::unique_ptr<ISampler> createSampler(const SamplerDesc & /*desc*/) override {
    return nullptr;
  }

  std::shared_ptr<IMaterial>
  createMaterial(const MaterialDesc &desc,
                 std::shared_ptr<ITexture> albedo,
                 std::shared_ptr<ITexture> normal,
                 std::shared_ptr<ITexture> metallicRoughness,
                 std::shared_ptr<ITexture> emissive,
                 std::shared_ptr<ITexture> occlusion) override {
    materialDescs_.push_back(desc);
    return std::make_shared<NullMaterial>(desc);
  }
  void setJobSystem(Jobs::JobSystem & /*js*/) override {}
  void waitIdle() override {}
  bool drawFrame(const RenderCallback & /*callback*/) override { return true; }

  int createCount_ = 0;
  std::vector<MaterialDesc> materialDescs_;
};

} // namespace Raiden::Renderer
