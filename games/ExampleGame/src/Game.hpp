#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Engine/IGamePlugin.hpp>
#include <Raiden/Input/ActionMap.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/ITexture.hpp>
#include <Raiden/Renderer/Model.hpp>

#include <memory>
#include <string>
#include <unordered_map>

class ExampleGame : public Raiden::Engine::IGamePlugin {
public:
  bool init(Raiden::Renderer::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs,
            Raiden::Assets::IAssetManager &assets,
            Raiden::Platform::IPlatform *platform,
            Raiden::Audio::IAudioDevice *audio = nullptr) override;

  void update(float deltaTime,
              const Raiden::Platform::InputState &input) override;
  void render(Raiden::Renderer::ICommandBuffer &cmd) override;
  void shutdown() override;
  void onDebugUI() override {}

  const char *name() const override { return "Example Game"; }
  Raiden::ECS::World *getWorld() override { return &world_; }
  bool shouldQuit() const { return quitRequested_; }

private:
  Raiden::ECS::World world_;
  Raiden::Input::ActionMap actions_;
  Raiden::Assets::IAssetManager *assets_ = nullptr;
  Raiden::Platform::IPlatform *platform_ = nullptr;
  Raiden::Renderer::IRenderDevice *device_ = nullptr;

  // main pipeline + resources
  std::unique_ptr<Raiden::Renderer::IPipeline> pipeline_;

  // resource caches (loaded from MeshRenderer paths)
  struct MeshCache {
    std::shared_ptr<Raiden::Renderer::Model> model;
    std::shared_ptr<Raiden::Renderer::ITexture> texture;
    std::shared_ptr<Raiden::Renderer::IMaterial> material;
  };
  std::unordered_map<std::string, MeshCache> meshCaches_;

  // skybox (special-cased, not ECS)
  std::unique_ptr<Raiden::Renderer::IPipeline> skyboxPipeline_;
  std::shared_ptr<Raiden::Renderer::ITexture> skyboxTexture_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxVertexBuffer_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxIndexBuffer_;
  uint32_t skyboxIndexCount_ = 0;

  bool quitRequested_ = false;

  // camera controls
  Raiden::ECS::Entity camEntity_;
  glm::vec3 position_ = {0.0F, 0.0F, 3.0F};
  float yaw_ = -1.5707963F;
  float pitch_ = 0.0F;
  bool mouseCaptured_ = false;

  float rotation_ = 0.0F;

  MeshCache &getOrCreateCache(const std::string &meshPath,
                              const std::string &texturePath,
                              const std::string &shader, float metallic,
                              float roughness,
                              const glm::vec4 &baseColorFactor);
};
