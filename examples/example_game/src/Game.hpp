#pragma once

#include <RaidenEngineCore/Core/IVirtualFileSystem.hpp>
#include <RaidenEngineCore/Engine/IGamePlugin.hpp>
#include <RaidenEngineCore/Input/ActionMap.hpp>
#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/IPipeline.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/ITexture.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#include <glm/glm.hpp>

#include <memory>

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 uv;
};

class ExampleGame final : public Raiden::Core::IGamePlugin {
public:
  const char *name() const override { return "Example Game"; }

  bool init(Raiden::Core::IRenderDevice &device,
            Raiden::Core::IVirtualFileSystem &vfs) override;

  void update(float deltaTime, const Raiden::Core::InputState &input) override;
  void render(Raiden::Core::ICommandBuffer &cmd) override;
  void shutdown() override;

  bool quitRequested() const { return quitRequested_; }

private:
  Raiden::Core::ActionMap actions_;
  
  std::unique_ptr<Raiden::Core::IPipeline> pipeline_;
  std::unique_ptr<Raiden::Core::IBuffer> vertexBuffer_;
  std::unique_ptr<Raiden::Core::IBuffer> indexBuffer_;
  std::unique_ptr<Raiden::Core::ITexture> texture_;

  uint32_t indexCount_ = 0;
  bool quitRequested_ = false;
};
