#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Engine/IGamePlugin.hpp>
#include <Raiden/Input/ActionMap.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IMaterial.hpp>
#include <Raiden/Renderer/IPipeline.hpp>
#include <Raiden/Renderer/ITexture.hpp>
#include <Raiden/Renderer/Model.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class VisualDemo : public Raiden::Engine::IGamePlugin {
public:
  bool init(Raiden::Renderer::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs,
            Raiden::Assets::IAssetManager &assets,
            Raiden::Platform::IPlatform *platform,
            Raiden::Audio::IAudioDevice *audio = nullptr,
            Raiden::Physics::IPhysicsSystem *physics = nullptr) override;

  void update(float deltaTime,
              const Raiden::Platform::InputState &input) override;
  void render(Raiden::Renderer::ICommandBuffer &cmd) override;
  void shutdown() override;
  void onDebugUI() override;

  const char *name() const override { return "Visual Demos"; }
  Raiden::ECS::World *getWorld() override { return &world_; }

private:
  Raiden::ECS::World world_;
  Raiden::Input::ActionMap actions_;
  Raiden::Assets::IAssetManager *assets_ = nullptr;
  Raiden::Platform::IPlatform *platform_ = nullptr;
  Raiden::Renderer::IRenderDevice *device_ = nullptr;
  Raiden::Physics::IPhysicsSystem *physics_ = nullptr;

  // main pipeline
  std::unique_ptr<Raiden::Renderer::IPipeline> pipeline_;

  // resource cache
  struct PbrTextures {
    std::string albedo;
    std::string normal;
    std::string metallicRoughness;
    std::string occlusion;
    std::string emissive;
  };

  struct MeshCache {
    std::shared_ptr<Raiden::Renderer::Model> model;
    std::shared_ptr<Raiden::Renderer::ITexture> albedoTex;
    std::shared_ptr<Raiden::Renderer::ITexture> normalTex;
    std::shared_ptr<Raiden::Renderer::ITexture> mrTex;
    std::shared_ptr<Raiden::Renderer::ITexture> aoTex;
    std::shared_ptr<Raiden::Renderer::IMaterial> material;
  };
  std::unordered_map<std::string, MeshCache> meshCaches_;

  // skybox
  std::unique_ptr<Raiden::Renderer::IPipeline> skyboxPipeline_;
  std::shared_ptr<Raiden::Renderer::ITexture> skyboxTexture_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxVertexBuffer_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxIndexBuffer_;
  uint32_t skyboxIndexCount_ = 0;

  // camera
  Raiden::ECS::Entity camEntity_;
  glm::vec3 camPos_ = {0.0F, 5.0F, 12.0F};
  float yaw_ = -1.5707963F;
  float pitch_ = -0.3F;
  bool mouseCaptured_ = false;

  // box spawning
  int boxCount_ = 0;
  static constexpr int kMaxBoxes = 64;

  MeshCache &getOrCreateCache(const std::string &meshPath,
                               const PbrTextures &textures,
                               const std::string &shader, float metallic,
                               float roughness,
                               const glm::vec4 &baseColorFactor);

  void spawnBox();
  void resetBoxes();
};
