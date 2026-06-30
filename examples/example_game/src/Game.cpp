#include "Game.hpp"

#include <RaidenEngineCore/Assets/IAssetManager.hpp>
#include <RaidenEngineCore/ECS/Camera.hpp>
#include <RaidenEngineCore/ECS/Transform.hpp>
#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/IBuffer.hpp>
#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>
#include <RaidenEngineCore/Renderer/IRenderDevice.hpp>
#include <RaidenEngineCore/Renderer/RenderTypes.hpp>

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

  // load KTX2 texture through asset manager
  texture_ = assets.loadTexture("game://textures/checkerboard.ktx2");
  if (!texture_) {
    s_logger.error("Failed to load checkerboard texture");
    return false;
  }

  // load glTF cube model
  model_ = assets.loadMesh("game://meshes/cube.glb");

  if (!model_) {
    s_logger.error("Failed to load cube model");
    return false;
  }

  s_logger.info("Cube model loaded: {} meshes", model_->meshes.size());
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

  glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(rotation_),
                                glm::vec3(0.0f, 1.0f, 0.0f));
  cmd.pushConstants(0, sizeof(glm::mat4), &model);
  cmd.bindTexture(0, *texture_);

  for (auto &mesh : model_->meshes) {
    if (!mesh.isValid())
      continue;
    cmd.bindVertexBuffer(*mesh.vertexBuffer);
    cmd.bindIndexBuffer(*mesh.indexBuffer);
    cmd.drawIndexed(mesh.indexCount);
  }
}

void ExampleGame::shutdown() {
  s_logger.info("Shutting down example game...");

  pipeline_.reset();
  texture_.reset();
  model_.reset();
}

extern "C" {

Raiden::Core::IGamePlugin *raiden_create_plugin() { return new ExampleGame(); }

void raiden_destroy_plugin(Raiden::Core::IGamePlugin *plugin) {
  delete dynamic_cast<ExampleGame *>(plugin);
}
}