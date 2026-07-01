#pragma once

#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Engine/IGamePlugin.hpp>
#include <RaidenEngineCore/Input/ActionMap.hpp>
#include <RaidenEngineCore/Platform/IPlatform.hpp>
#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/IMaterial.hpp>
#include <RaidenEngineCore/Renderer/IPipeline.hpp>
#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/Model.hpp>

#include <memory>
#include <vector>

class ExampleGame : public Raiden::Core::IGamePlugin {
public:
  bool init(Raiden::Core::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs,
            Raiden::Core::IAssetManager &assets,
            Raiden::Core::IPlatform *platform) override;

  void update(float deltaTime, const Raiden::Core::InputState &input) override;
  void render(Raiden::Core::ICommandBuffer &cmd) override;
  void shutdown() override;
  void onDebugUI() override {}

  const char *name() const override { return "Example Game"; }
  Raiden::Core::World *getWorld() override { return &world_; }
  bool shouldQuit() const { return quitRequested_; }

private:
  Raiden::Core::World world_;
  Raiden::Core::ActionMap actions_;
  Raiden::Core::IAssetManager *assets_ = nullptr;
  Raiden::Core::IPlatform *platform_ = nullptr;

  struct PbrObject {
    glm::vec3 position;
    float rotation;
    std::shared_ptr<Raiden::Core::IMaterial> material;
  };

  std::unique_ptr<Raiden::Core::IPipeline> pipeline_;
  std::shared_ptr<Raiden::Core::ITexture> texture_;
  std::shared_ptr<Raiden::Core::Model> model_;

  std::vector<PbrObject> pbrObjects_;

  float rotation_ = 0.0f;
  bool quitRequested_ = false;

  // camera controls
  Raiden::Core::Entity camEntity_;
  glm::vec3 position_ = {0.0f, 0.0f, 3.0f};
  float yaw_ = -1.5707963f; // -90 degrees, looking along -Z
  float pitch_ = 0.0f;
  bool mouseCaptured_ = false;
};