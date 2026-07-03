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
#include <vector>

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

  struct PbrObject {
    glm::vec3 position;
    float rotation;
    std::shared_ptr<Raiden::Renderer::IMaterial> material;
  };

  std::unique_ptr<Raiden::Renderer::IPipeline> pipeline_;
  std::shared_ptr<Raiden::Renderer::ITexture> texture_;
  std::shared_ptr<Raiden::Renderer::Model> model_;

  // skybox
  std::unique_ptr<Raiden::Renderer::IPipeline> skyboxPipeline_;
  std::shared_ptr<Raiden::Renderer::ITexture> skyboxTexture_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxVertexBuffer_;
  std::unique_ptr<Raiden::Renderer::IBuffer> skyboxIndexBuffer_;
  uint32_t skyboxIndexCount_ = 0;

  std::vector<PbrObject> pbrObjects_;

  float rotation_ = 0.0f;
  bool quitRequested_ = false;

  // camera controls
  Raiden::ECS::Entity camEntity_;
  glm::vec3 position_ = {0.0f, 0.0f, 3.0f};
  float yaw_ = -1.5707963f; // -90 degrees, looking along -Z
  float pitch_ = 0.0f;
  bool mouseCaptured_ = false;
};
