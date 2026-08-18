#pragma once

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
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

#include "Systems/CameraControllerSystem.hpp"

#include <memory>
#include <string>
#include <unordered_map>

class NewVisualDemo : public Raiden::Engine::IGamePlugin {
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

  const char *name() const override { return "New Visual Demo"; }
  Raiden::ECS::World *getWorld() override { return &world_; }

private:
  Raiden::ECS::World world_;
  Raiden::Input::ActionMap actions_;
  Raiden::Assets::IAssetManager *assets_ = nullptr;
  Raiden::Core::IVirtualFileSystem *vfs_ = nullptr;
  Raiden::Platform::IPlatform *platform_ = nullptr;
  Raiden::Renderer::IRenderDevice *device_ = nullptr;
  Raiden::Physics::IPhysicsSystem *physics_ = nullptr;
  Raiden::Audio::IAudioDevice *audio_ = nullptr;

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

  std::unique_ptr<Raiden::Renderer::IPipeline> skyboxPipeline_;
  std::shared_ptr<Raiden::Renderer::ITexture> skyboxTexture_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxVertexBuffer_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxIndexBuffer_;
  uint32_t skyboxIndexCount_ = 0;

  Raiden::ECS::Entity camEntity_ = Raiden::ECS::NullEntity;
  CameraControllerSystem *cameraSystem_ = nullptr;
  glm::vec3 camPos_ = {0.0F, 2.0F, 8.0F};
  float yaw_ = -1.5707963F;
  float pitch_ = -0.1F;
  bool mouseCaptured_ = false;
  uint32_t playerBodyId_ = Raiden::Physics::kNoBody;

  int boxCount_ = 0;
  static constexpr int kMaxBoxes = 32;

  MeshCache &getOrCreateCache(const std::string &meshPath,
                              const PbrTextures &textures,
                              float metallic, float roughness,
                              const glm::vec4 &baseColorFactor,
                              const glm::vec2 &uvScale,
                              bool triplanarMapping);

  void spawnBox();
  void resetBoxes();
};
