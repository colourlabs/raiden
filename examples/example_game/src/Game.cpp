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

// simple demo with PBR lighting, KTX2 textures and a glTF cube model + example
// first person camera + movement

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Core::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Core::IAssetManager &assets,
                       Raiden::Core::IPlatform *platform,
                       Raiden::Core::IAudioDevice *audio) {
  // unsed for now
  (void)audio;
  platform_ = platform;
  s_logger.info("Initializing example game...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // camera
  camEntity_ = world_.create();
  world_.assign<Raiden::Core::Camera>(camEntity_);
  auto &cam = world_.get<Raiden::Core::Camera>(camEntity_);
  cam.setLookAt({0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f});
  cam.setPerspective(45.0f, 1.0f, 0.1f, 100.0f);

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

  // PBR test objects

  struct PbrPreset {
    glm::vec3 position;
    glm::vec4 color;
    float metallic;
    float roughness;
    const char *label;
  };

  PbrPreset presets[] = {
      {{1.0f, 0.0f, 0.0f},
       {1.0f, 0.2f, 0.2f, 1.0f},
       0.0f,
       0.8f,
       "rough dielectric"},
      {{-1.0f, 0.0f, 0.0f},
       {0.2f, 0.4f, 1.0f, 1.0f},
       0.0f,
       0.2f,
       "smooth dielectric"},
      {{0.0f, 0.8f, 0.0f},
       {0.8f, 0.8f, 0.8f, 1.0f},
       0.9f,
       0.4f,
       "brushed metal"},
      {{0.0f, -0.8f, 0.0f},
       {1.0f, 0.8f, 0.2f, 1.0f},
       0.9f,
       0.1f,
       "polished metal"},
  };

  for (auto &p : presets) {
    Raiden::Core::MaterialDesc matDesc;
    matDesc.shader = "builtin://pbr";
    matDesc.baseColorFactor = p.color;
    matDesc.metallicFactor = p.metallic;
    matDesc.roughnessFactor = p.roughness;

    // no texture paths -> uses white fallback texture, visible color comes from
    // baseColorFactor
    auto mat = device.createMaterial(matDesc, nullptr, nullptr, nullptr,
                                     nullptr, nullptr);
    if (mat) {
      pbrObjects_.push_back({p.position, 0.0f, std::move(mat)});
    } else {
      s_logger.error("Failed to create PBR material for '{}'", p.label);
    }
  }

  s_logger.info("Example game initialized ({} PBR objects).",
                pbrObjects_.size());

  return true;
}

void ExampleGame::update(float deltaTime,
                         const Raiden::Core::InputState &input) {
  actions_.evaluate(input);

  if (auto *quit = actions_.find("quit"); quit && quit->justPressed) {
    quitRequested_ = true;
  }

  // toggle mouse capture on right-click
  static bool prevRmb = false;
  if (input.mouseButtons[2] && !prevRmb) {
    bool wasCaptured = mouseCaptured_;
    mouseCaptured_ = !wasCaptured;
    platform_->setRelativeMouseMode(mouseCaptured_);
  }
  prevRmb = input.mouseButtons[2];

  if (mouseCaptured_) {
    float const sensitivity = 0.002f;
    yaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0f), glm::radians(89.0f));
  }

  // movement
  float speed = 3.0f * deltaTime;
  if (auto *fw = actions_.find("move_forward"); fw && fw->pressed) {
    speed *= 2.0f; // sprint
  }

  glm::vec3 forward(std::cos(yaw_) * std::cos(pitch_), std::sin(pitch_),
                    std::sin(yaw_) * std::cos(pitch_));
  forward = glm::normalize(forward);

  glm::vec3 right = glm::normalize(glm::cross(forward, {0.0f, 1.0f, 0.0f}));
  glm::vec3 up = glm::cross(right, forward);

  if (auto *mv = actions_.find("move_forward"); mv && mv->pressed)
    position_ += forward * speed;
  if (auto *mv = actions_.find("move_back"); mv && mv->pressed)
    position_ -= forward * speed;
  if (auto *mv = actions_.find("move_left"); mv && mv->pressed)
    position_ -= right * speed;
  if (auto *mv = actions_.find("move_right"); mv && mv->pressed)
    position_ += right * speed;
  if (auto *mv = actions_.find("move_up"); mv && mv->pressed)
    position_ += up * speed;
  if (auto *mv = actions_.find("move_down"); mv && mv->pressed)
    position_ -= up * speed;

  auto &cam = world_.get<Raiden::Core::Camera>(camEntity_);
  cam.view = glm::lookAt(position_, position_ + forward, {0.0f, 1.0f, 0.0f});

  rotation_ += deltaTime * 45.0f;

  for (auto &obj : pbrObjects_)
    obj.rotation += deltaTime * 45.0f;
}

void ExampleGame::render(Raiden::Core::ICommandBuffer &cmd) {
  // simple pipeline cube (rotating, checkerboard)
  cmd.bindPipeline(*pipeline_);

  float const s = 0.5f;
  glm::mat4 simpleModel = glm::scale(glm::mat4(1.0f), glm::vec3(s)) *
                          glm::rotate(glm::mat4(1.0f), glm::radians(rotation_),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
  cmd.pushConstants(0, sizeof(glm::mat4), &simpleModel);
  cmd.bindTexture(0, *texture_);

  for (auto &mesh : model_->meshes) {
    if (!mesh.isValid())
      continue;
    cmd.bindVertexBuffer(*mesh.vertexBuffer);
    cmd.bindIndexBuffer(*mesh.indexBuffer);
    cmd.drawIndexed(mesh.indexCount);
  }

  // PBR material cubes
  for (auto &obj : pbrObjects_) {
    obj.material->bind(cmd);

    glm::mat4 m = glm::translate(glm::mat4(1.0f), obj.position) *
                  glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)) *
                  glm::rotate(glm::mat4(1.0f), glm::radians(obj.rotation),
                              glm::vec3(0.0f, 1.0f, 0.0f));

    cmd.pushConstants(0, sizeof(glm::mat4), &m);

    for (auto &mesh : model_->meshes) {
      if (!mesh.isValid())
        continue;
      cmd.bindVertexBuffer(*mesh.vertexBuffer);
      cmd.bindIndexBuffer(*mesh.indexBuffer);
      cmd.drawIndexed(mesh.indexCount);
    }
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
