#include "Game.hpp"

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/PluginABI.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

// simple demo with PBR lighting, KTX2 textures and a glTF cube model + example
// first person camera + movement

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Renderer::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Assets::IAssetManager &assets,
                       Raiden::Platform::IPlatform *platform,
                       Raiden::Audio::IAudioDevice *audio) {
  // unsed for now
  (void)audio;

  platform_ = platform;
  s_logger.info("Initializing example game...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // camera
  camEntity_ = world_.create();
  world_.assign<Raiden::ECS::Camera>(camEntity_);

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);

  cam.setLookAt({0.0F, 0.0F, 3.0F}, {0.0F, 0.0F, 0.0F});
  cam.setPerspective(45.0F, 1.0F, 0.1F, 100.0F);

  // pipeline
  Raiden::Renderer::PipelineDesc pipelineDesc{
      .shader = {"shaders/triangle.slang"},
      .vertexLayout =
          {
              .stride = sizeof(Raiden::Renderer::Vertex),
              .attributes =
                  {
                      {.location = 0,
                       .format = Raiden::Renderer::Format::R32G32B32_Float,
                       .offset = offsetof(Raiden::Renderer::Vertex, pos)},
                      {.location = 1,
                       .format = Raiden::Renderer::Format::R32G32B32_Float,
                       .offset = offsetof(Raiden::Renderer::Vertex, normal)},
                      {.location = 2,
                       .format = Raiden::Renderer::Format::R32G32B32_Float,
                       .offset = offsetof(Raiden::Renderer::Vertex, color)},
                      {.location = 3,
                       .format = Raiden::Renderer::Format::R32G32_Float,
                       .offset = offsetof(Raiden::Renderer::Vertex, uv)},
                  },
          },
      .depthTestEnable = true,
  };

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create pipeline");
    return false;
  }

  // load KTX2 texture through asset manager (synchronous for boot assets)
  texture_ = assets.loadTextureSync("game://textures/checkerboard.ktx2");
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

  std::array<PbrPreset, 4> presets = {{
      {.position = {1.0F, 0.0F, 0.0F},
       .color = {1.0F, 0.2F, 0.2F, 1.0F},
       .metallic = 0.0F,
       .roughness = 0.8F,
       .label = "rough dielectric"},
      {.position = {-1.0F, 0.0F, 0.0F},
       .color = {0.2F, 0.4F, 1.0F, 1.0F},
       .metallic = 0.0F,
       .roughness = 0.2F,
       .label = "smooth dielectric"},
      {.position = {0.0F, 0.8F, 0.0F},
       .color = {0.8F, 0.8F, 0.8F, 1.0F},
       .metallic = 0.9F,
       .roughness = 0.4F,
       .label = "brushed metal"},
      {.position = {0.0F, -0.8F, 0.0F},
       .color = {1.0F, 0.8F, 0.2F, 1.0F},
       .metallic = 0.9F,
       .roughness = 0.1F,
       .label = "polished metal"},
  }};

  for (auto &p : presets) {
    Raiden::Renderer::MaterialDesc matDesc;
    matDesc.shader = "builtin://pbr";
    matDesc.baseColorFactor = p.color;
    matDesc.metallicFactor = p.metallic;
    matDesc.roughnessFactor = p.roughness;

    // no texture paths -> uses white fallback texture, visible color comes from
    // baseColorFactor
    auto mat = device.createMaterial(matDesc, nullptr, nullptr, nullptr,
                                     nullptr, nullptr);
    if (mat) {
      pbrObjects_.push_back({p.position, 0.0F, std::move(mat)});
    } else {
      s_logger.error("Failed to create PBR material for '{}'", p.label);
    }
  }

  s_logger.info("Example game initialized ({} PBR objects).",
                pbrObjects_.size());

  // skybox

  // unit cube vertices (just position, 36 vertices for indexed drawing)
  struct Pos {
    float x, y, z;
  };

  std::array<Pos, 8> cubeVerts = {{
      {.x = -1, .y = -1, .z = -1},
      {.x = 1, .y = -1, .z = -1},
      {.x = 1, .y = 1, .z = -1},
      {.x = -1, .y = 1, .z = -1},
      {.x = -1, .y = -1, .z = 1},
      {.x = 1, .y = -1, .z = 1},
      {.x = 1, .y = 1, .z = 1},
      {.x = -1, .y = 1, .z = 1},
  }};

  std::array<uint32_t, 36> cubeIndices = {{
      0, 1, 2, 0, 2, 3,       // back
      4, 6, 5, 4, 7, 6,     // front
      3, 7, 4, 3, 4, 0, // left
      1, 5, 6, 1, 6, 2, // right
      3, 2, 6, 3, 6, 7, // top
      0, 4, 5, 0, 5, 1, // bottom
  }};

  skyboxIndexCount_ = 36;

  skyboxVertexBuffer_ = device.createBuffer({
      .size = sizeof(cubeVerts),
      .usage = Raiden::Renderer::BufferUsage::Vertex,
      .access = Raiden::Renderer::MemoryAccess::CpuToGpu,
  });

  if (skyboxVertexBuffer_) {
    skyboxVertexBuffer_->upload(cubeVerts.data(), sizeof(cubeVerts));
  }

  skyboxIndexBuffer_ = device.createBuffer({
      .size = sizeof(cubeIndices),
      .usage = Raiden::Renderer::BufferUsage::Index,
      .access = Raiden::Renderer::MemoryAccess::CpuToGpu,
      .indexType = Raiden::Renderer::IndexType::Uint32,
  });

  if (skyboxIndexBuffer_) {
    skyboxIndexBuffer_->upload(cubeIndices.data(), sizeof(cubeIndices));
  }

  // load real skybox texture
  skyboxTexture_ = assets.loadTextureSync("game://textures/skybox.ktx2");
  if (!skyboxTexture_) {
    s_logger.warn("Failed to load skybox cubemap texture, continuing without");
  }

  // skybox pipeline: position-only vertex, depth test LEQUAL, no depth write,
  // no culling
  skyboxPipeline_ = device.createPipeline({
      .shader = {"shaders/skybox.slang"},
      .vertexLayout =
          {
              .stride = sizeof(Pos),
              .attributes =
                  {
                      {.location = 0,
                       .format = Raiden::Renderer::Format::R32G32B32_Float,
                       .offset = offsetof(Pos, x)},
                  },
          },
      .depthTestEnable = true,
      .depthWriteEnable = false,
      .cullMode = Raiden::Renderer::CullMode::None,
      .depthCompareOp = Raiden::Renderer::CompareOp::LessOrEqual,
  });

  return true;
}

void ExampleGame::update(float deltaTime,
                         const Raiden::Platform::InputState &input) {
  actions_.evaluate(input);

  if (const auto *quit = actions_.find("quit");
      (quit != nullptr) && quit->justPressed) {
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
    float const sensitivity = 0.002F;
    yaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  // movement
  float speed = 3.0F * deltaTime;
  if (const auto *fw = actions_.find("move_forward");
      (fw != nullptr) && fw->pressed) {
    speed *= 2.0F; // sprint
  }

  glm::vec3 forward(std::cos(yaw_) * std::cos(pitch_), std::sin(pitch_),
                    std::sin(yaw_) * std::cos(pitch_));
  forward = glm::normalize(forward);

  glm::vec3 right = glm::normalize(glm::cross(forward, {0.0F, 1.0F, 0.0F}));
  glm::vec3 up = glm::cross(right, forward);

  if (const auto *mv = actions_.find("move_forward");
      (mv != nullptr) && mv->pressed) {
    position_ += forward * speed;
  }

  if (const auto *mv = actions_.find("move_back");
      (mv != nullptr) && mv->pressed) {
    position_ -= forward * speed;
  }

  if (const auto *mv = actions_.find("move_left");
      (mv != nullptr) && mv->pressed) {
    position_ -= right * speed;
  }

  if (const auto *mv = actions_.find("move_right");
      (mv != nullptr) && mv->pressed) {
    position_ += right * speed;
  }

  if (const auto *mv = actions_.find("move_up");
      (mv != nullptr) && mv->pressed) {
    position_ += up * speed;
  }

  if (const auto *mv = actions_.find("move_down");
      (mv != nullptr) && mv->pressed) {
    position_ -= up * speed;
  }

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);
  cam.view = glm::lookAt(position_, position_ + forward, {0.0F, 1.0F, 0.0F});

  rotation_ += deltaTime * 45.0F;

  for (auto &obj : pbrObjects_) {
    obj.rotation += deltaTime * 45.0F;
  }
}

void ExampleGame::render(Raiden::Renderer::ICommandBuffer &cmd) {
  // skybox (rendered first with depth test LEQUAL, no depth write)
  if (skyboxPipeline_ && skyboxTexture_ && skyboxVertexBuffer_ &&
      skyboxIndexBuffer_) {
    cmd.bindPipeline(*skyboxPipeline_);
    cmd.bindTexture(0, *skyboxTexture_);
    cmd.bindVertexBuffer(*skyboxVertexBuffer_);
    cmd.bindIndexBuffer(*skyboxIndexBuffer_);
    cmd.drawIndexed(skyboxIndexCount_);
  }

  // simple pipeline cube (rotating, checkerboard)
  cmd.bindPipeline(*pipeline_);

  float const s = 0.5F;
  glm::mat4 simpleModel = glm::scale(glm::mat4(1.0F), glm::vec3(s)) *
                          glm::rotate(glm::mat4(1.0F), glm::radians(rotation_),
                                      glm::vec3(0.0F, 1.0F, 0.0F));

  cmd.pushConstants(0, sizeof(glm::mat4), &simpleModel);
  cmd.bindTexture(0, *texture_);

  for (auto &mesh : model_->meshes) {
    if (!mesh.isValid()) {
      continue;
    }

    cmd.bindVertexBuffer(*mesh.vertexBuffer);
    cmd.bindIndexBuffer(*mesh.indexBuffer);
    cmd.drawIndexed(mesh.indexCount);
  }

  // PBR material cubes
  for (auto &obj : pbrObjects_) {
    obj.material->bind(cmd);

    glm::mat4 m = glm::translate(glm::mat4(1.0F), obj.position) *
                  glm::scale(glm::mat4(1.0F), glm::vec3(0.5F)) *
                  glm::rotate(glm::mat4(1.0F), glm::radians(obj.rotation),
                              glm::vec3(0.0F, 1.0F, 0.0F));

    cmd.pushConstants(0, sizeof(glm::mat4), &m);

    for (auto &mesh : model_->meshes) {
      if (!mesh.isValid()) {
        continue;
      }

      cmd.bindVertexBuffer(*mesh.vertexBuffer);
      cmd.bindIndexBuffer(*mesh.indexBuffer);
      cmd.drawIndexed(mesh.indexCount);
    }
  }
}

void ExampleGame::shutdown() {
  s_logger.info("Shutting down example game...");

  pbrObjects_.clear();
  skyboxPipeline_.reset();
  skyboxTexture_.reset();
  skyboxVertexBuffer_.reset();
  skyboxIndexBuffer_.reset();
  pipeline_.reset();
  texture_.reset();
  model_.reset();
}

extern "C" {

RAIDEN_EXPORT Raiden::Engine::IGamePlugin *raiden_create_plugin() {
  return new ExampleGame();
}

RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Engine::IGamePlugin *plugin) {
  delete plugin;
}
}
