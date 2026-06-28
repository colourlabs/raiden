#include "Game.hpp"

#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/ECS/Camera.hpp>
#include <RaidenEngineCore/ECS/Transform.hpp>
#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

#include <array>
#include <cstdint>

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Core::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Core::IAssetManager &assets) {
  s_logger.info("Initializing example game...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // camera
  auto camEntity = world_.create();
  world_.assign<Raiden::Core::Camera>(camEntity);
  auto &cam = world_.get<Raiden::Core::Camera>(camEntity);
  cam.setLookAt({0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f});
  cam.setPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);

  // pipeline
  Raiden::Core::PipelineDesc pipelineDesc{
      .shader = {"shaders/triangle.slang"},
      .vertexLayout =
          {
              .stride = sizeof(Raiden::Core::Vertex),
              .attributes =
                  {
                      {0, Raiden::Core::Format::R32G32B32_Float,
                       offsetof(Raiden::Core::Vertex, pos)},
                      {1, Raiden::Core::Format::R32G32B32_Float,
                       offsetof(Raiden::Core::Vertex, normal)},
                      {2, Raiden::Core::Format::R32G32B32_Float,
                       offsetof(Raiden::Core::Vertex, color)},
                      {3, Raiden::Core::Format::R32G32_Float,
                       offsetof(Raiden::Core::Vertex, uv)},
                  },
          },
      .depthTestEnable = true,
  };

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create pipeline");
    return false;
  }

  // quad vertices
  std::array<Raiden::Core::Vertex, 4> vertices = {{
      {{-0.7f, -0.7f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},
      {{0.7f, -0.7f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},
      {{0.7f, 0.7f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},
      {{-0.7f, 0.7f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},
  }};

  vertexBuffer_ =
      device.createBuffer({sizeof(vertices), Raiden::Core::BufferUsage::Vertex,
                           Raiden::Core::MemoryAccess::CpuToGpu});
  if (!vertexBuffer_) {
    s_logger.error("Failed to create vertex buffer");
    return false;
  }
  vertexBuffer_->upload(vertices.data(), sizeof(vertices));

  std::array<uint16_t, 6> indices = {{0, 1, 2, 0, 2, 3}};
  indexBuffer_ =
      device.createBuffer({sizeof(indices), Raiden::Core::BufferUsage::Index,
                           Raiden::Core::MemoryAccess::CpuToGpu});
  if (!indexBuffer_) {
    s_logger.error("Failed to create index buffer");
    return false;
  }
  indexBuffer_->upload(indices.data(), sizeof(indices));
  indexCount_ = 6;

  // load KTX2 texture through asset manager
  texture_ = assets.loadTexture("game://textures/checkerboard.ktx2");
  if (!texture_) {
    s_logger.error("Failed to load checkerboard texture");
    return false;
  }

  s_logger.info("Example game initialized.");
  return true;
}

void ExampleGame::update(float deltaTime,
                         const Raiden::Core::InputState &input) {
  actions_.evaluate(input);

  if (auto *quit = actions_.find("quit"); quit && quit->justPressed) {
    quitRequested_ = true;
  }

  rotation_ += deltaTime * 45.0f;
}

void ExampleGame::render(Raiden::Core::ICommandBuffer &cmd) {
  cmd.bindPipeline(*pipeline_);
  cmd.bindVertexBuffer(*vertexBuffer_);
  cmd.bindIndexBuffer(*indexBuffer_);
  cmd.bindTexture(0, *texture_);
  cmd.drawIndexed(indexCount_);
}

void ExampleGame::shutdown() {
  s_logger.info("Shutting down example game...");

  pipeline_.reset();
  vertexBuffer_.reset();
  indexBuffer_.reset();
  texture_.reset();
}

extern "C" {

Raiden::Core::IGamePlugin *raiden_create_plugin() { return new ExampleGame(); }

void raiden_destroy_plugin(Raiden::Core::IGamePlugin *plugin) {
  delete dynamic_cast<ExampleGame *>(plugin);
}
}