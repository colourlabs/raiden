#pragma once

#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Engine/IGamePlugin.hpp>
#include <RaidenEngineCore/Input/ActionMap.hpp>
#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/IPipeline.hpp>
#include <RaidenEngineCore/Renderer/ITexture.hpp>

#include <memory>

class ExampleGame : public Raiden::Core::IGamePlugin {
public:
  bool init(Raiden::Core::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs,
            Raiden::Core::IAssetManager &assets) override;

  void update(float deltaTime, const Raiden::Core::InputState &input) override;
  void render(Raiden::Core::ICommandBuffer &cmd) override;
  void shutdown() override;
  void onDebugUI() override {}

  const char *name() const override { return "Example Game"; }
  bool shouldQuit() const { return quitRequested_; }

private:
  Raiden::Core::World world_;
  Raiden::Core::ActionMap actions_;
  Raiden::Core::IAssetManager *assets_ = nullptr;

  std::unique_ptr<Raiden::Core::IPipeline> pipeline_;
  std::unique_ptr<Raiden::Core::IBuffer> vertexBuffer_;
  std::unique_ptr<Raiden::Core::IBuffer> indexBuffer_;
  std::shared_ptr<Raiden::Core::ITexture>  texture_;
  
  uint32_t indexCount_ = 0;
  
  float rotation_ = 0.0f;
  bool quitRequested_ = false;
};