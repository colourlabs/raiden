#include "Game.hpp"

#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Platform/InputState.hpp>
#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>

#include <array>
#include <cstdint>

// TODO: actual gameplay and KTX assets

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Core::IRenderDevice &device,
                      Raiden::Core::IVirtualFileSystem &vfs) {
  s_logger.info("Initializing example game...");

  // load action bindings from the datapack VFS
  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // describe the pipeline
  Raiden::Core::PipelineDesc pipelineDesc{
      .shader = {"shaders/triangle.slang"},
      .vertexLayout =
          {
              .stride = sizeof(Vertex),
              .attributes =
                  {
                      {0, Raiden::Core::Format::R32G32_Float,
                       offsetof(Vertex, pos)},
                      {1, Raiden::Core::Format::R32G32B32_Float,
                       offsetof(Vertex, color)},
                  },
          },
      .depthTestEnable = true,
  };

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create pipeline");
    return false;
  }

  // vertex buffer
  std::array<Vertex, 3> vertices = {{
      {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
  }};

  vertexBuffer_ =
      device.createBuffer({sizeof(vertices), Raiden::Core::BufferUsage::Vertex,
                           Raiden::Core::MemoryAccess::CpuToGpu});
  if (!vertexBuffer_) {
    s_logger.error("Failed to create vertex buffer");
    return false;
  }
  vertexBuffer_->upload(vertices.data(), sizeof(vertices));

  // index buffer
  std::array<uint16_t, 3> indices = {{0, 1, 2}};
  indexBuffer_ =
      device.createBuffer({sizeof(indices), Raiden::Core::BufferUsage::Index,
                           Raiden::Core::MemoryAccess::CpuToGpu});
  if (!indexBuffer_) {
    s_logger.error("Failed to create index buffer");
    return false;
  }
  indexBuffer_->upload(indices.data(), sizeof(indices));
  indexCount_ = 3;

  s_logger.info("Example game initialized.");
  return true;
}

void ExampleGame::update(float /*deltaTime*/,
                         const Raiden::Core::InputState &input) {
  actions_.evaluate(input);

  if (auto *quit = actions_.find("quit"); quit && quit->justPressed) {
    quitRequested_ = true;
  }
}

void ExampleGame::render(Raiden::Core::ICommandBuffer &cmd) {
  cmd.bindPipeline(*pipeline_);
  cmd.bindVertexBuffer(*vertexBuffer_);
  cmd.bindIndexBuffer(*indexBuffer_);
  cmd.drawIndexed(indexCount_);
}

void ExampleGame::shutdown() {
  s_logger.info("Shutting down example game...");
  pipeline_.reset();
  vertexBuffer_.reset();
  indexBuffer_.reset();
}

extern "C" {

Raiden::Core::IGamePlugin *raiden_create_plugin() { return new ExampleGame(); }

void raiden_destroy_plugin(Raiden::Core::IGamePlugin *plugin) {
  delete static_cast<ExampleGame *>(plugin);
}

}

