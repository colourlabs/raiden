#include "Game.hpp"

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/PluginABI.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Renderer::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Assets::IAssetManager &assets,
                       Raiden::Platform::IPlatform *platform,
                       Raiden::Audio::IAudioDevice *audio) {
  (void)audio;

  platform_ = platform;
  device_ = &device;
  assets_ = &assets;
  s_logger.info("Initializing example game...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // camera
  camEntity_ = world_.create();
  
  world_.assign<Raiden::ECS::Name>(camEntity_, "Main Camera");
  world_.assign<Raiden::ECS::Camera>(camEntity_);
  world_.assign<Raiden::ECS::Transform>(camEntity_, Raiden::ECS::Transform{
    .translation = {0.0F, 0.0F, 3.0F},
  });

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);
  cam.setLookAt({0.0F, 0.0F, 3.0F}, {0.0F, 0.0F, 0.0F});
  cam.setPerspective(45.0F, 1.0F, 0.1F, 100.0F);

  // main pipeline
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

  // create ECS entities for the scene
  // rotating checkerboard cube
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Checkerboard Cube");
    world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
      .translation = {0.0F, 0.0F, 0.0F},
      .scale = {0.5F, 0.5F, 0.5F},
    });
    world_.assign<Raiden::ECS::MeshRenderer>(e, Raiden::ECS::MeshRenderer{
      .meshPath = "game://meshes/cube.glb",
      .texturePath = "game://textures/checkerboard.ktx2",
    });
  }

  // PBR cubes
  struct PbrPreset {
    const char *name;
    glm::vec3 position;
    glm::vec4 color;
    float metallic;
    float roughness;
  };

  std::array<PbrPreset, 4> presets = {{
      {.name = "Rough Cube",
       .position = {1.0F, 0.0F, 0.0F},
       .color = {1.0F, 0.2F, 0.2F, 1.0F},
       .metallic = 0.0F,
       .roughness = 0.8F},
      {.name = "Smooth Cube",
       .position = {-1.0F, 0.0F, 0.0F},
       .color = {0.2F, 0.4F, 1.0F, 1.0F},
       .metallic = 0.0F,
       .roughness = 0.2F},
      {.name = "Brushed Metal",
       .position = {0.0F, 0.8F, 0.0F},
       .color = {0.8F, 0.8F, 0.8F, 1.0F},
       .metallic = 0.9F,
       .roughness = 0.4F},
      {.name = "Polished Metal",
       .position = {0.0F, -0.8F, 0.0F},
       .color = {1.0F, 0.8F, 0.2F, 1.0F},
       .metallic = 0.9F,
       .roughness = 0.1F},
  }};

  for (auto &p : presets) {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, p.name);
    world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
      .translation = p.position,
      .scale = {0.5F, 0.5F, 0.5F},
    });
    world_.assign<Raiden::ECS::MeshRenderer>(e, Raiden::ECS::MeshRenderer{
      .meshPath = "game://meshes/cube.glb",
      .texturePath = "",
      .shader = "builtin://pbr",
      .baseColorFactor = p.color,
      .metallic = p.metallic,
      .roughness = p.roughness,
    });
  }

  // skybox
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
      0, 1, 2, 0, 2, 3,
      4, 6, 5, 4, 7, 6,
      3, 7, 4, 3, 4, 0,
      1, 5, 6, 1, 6, 2,
      3, 2, 6, 3, 6, 7,
      0, 4, 5, 0, 5, 1,
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

  skyboxTexture_ = assets.loadTextureSync("game://textures/skybox.ktx2");
  if (!skyboxTexture_) {
    s_logger.warn("Failed to load skybox cubemap texture");
  }

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

  s_logger.info("Example game initialized.");
  return true;
}

ExampleGame::MeshCache &ExampleGame::getOrCreateCache(
    const std::string &meshPath, const std::string &texturePath,
    const std::string &shader, float metallic, float roughness,
    const glm::vec4 &baseColorFactor) {
  auto key = meshPath + ":" + shader + ":" + texturePath;
  auto it = meshCaches_.find(key);
  if (it != meshCaches_.end()) {
    return it->second;
  }

  MeshCache cache;
  cache.model = assets_->loadMesh(meshPath);
  if (!cache.model) {
    s_logger.error("Failed to load mesh '{}'", meshPath);
  }

  if (!texturePath.empty()) {
    cache.texture = assets_->loadTextureSync(texturePath);
    if (!cache.texture) {
      s_logger.error("Failed to load texture '{}'", texturePath);
    }
  }

  Raiden::Renderer::MaterialDesc matDesc;
  matDesc.shader = shader;
  matDesc.baseColorFactor = baseColorFactor;
  matDesc.metallicFactor = metallic;
  matDesc.roughnessFactor = roughness;
  cache.material = device_->createMaterial(matDesc, cache.texture, nullptr,
                                           nullptr, nullptr, nullptr);
  if (!cache.material) {
    s_logger.error("Failed to create material for '{}'", meshPath);
  }

  auto [inserted, _] = meshCaches_.emplace(key, std::move(cache));
  return inserted->second;
}

void ExampleGame::update(float deltaTime,
                         const Raiden::Platform::InputState &input) {
  actions_.evaluate(input);

  if (const auto *quit = actions_.find("quit");
      (quit != nullptr) && quit->justPressed) {
    quitRequested_ = true;
  }

  static bool prevRmb = false;
  if (input.mouseButtons[2] && !prevRmb) {
    mouseCaptured_ = !mouseCaptured_;
    platform_->setRelativeMouseMode(mouseCaptured_);
  }
  prevRmb = input.mouseButtons[2];

  if (mouseCaptured_) {
    float const sensitivity = 0.002F;
    yaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  float speed = 3.0F * deltaTime;
  if (const auto *fw = actions_.find("move_forward");
      (fw != nullptr) && fw->pressed) {
    speed *= 2.0F;
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

  // update rotating objects
  world_.view<Raiden::ECS::Name, Raiden::ECS::Transform>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::Name &n, Raiden::ECS::Transform &t) {
        if (n.value == "Checkerboard Cube") {
          t.rotation = glm::angleAxis(glm::radians(rotation_),
                                      glm::vec3(0.0F, 1.0F, 0.0F));
        }
      });

  Raiden::ECS::updateTransforms(world_);
}

void ExampleGame::render(Raiden::Renderer::ICommandBuffer &cmd) {
  // skybox
  if (skyboxPipeline_ && skyboxTexture_ && skyboxVertexBuffer_ &&
      skyboxIndexBuffer_) {
    cmd.bindPipeline(*skyboxPipeline_);
    cmd.bindTexture(0, *skyboxTexture_);
    cmd.bindVertexBuffer(*skyboxVertexBuffer_);
    cmd.bindIndexBuffer(*skyboxIndexBuffer_);
    cmd.drawIndexed(skyboxIndexCount_);
  }

  // render all MeshRenderer entities
  world_.view<Raiden::ECS::Transform, Raiden::ECS::MeshRenderer>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::Transform &t,
          Raiden::ECS::MeshRenderer &mr) {
        auto &cache = getOrCreateCache(mr.meshPath, mr.texturePath, mr.shader,
                                       mr.metallic, mr.roughness,
                                       mr.baseColorFactor);

        if (cache.material) {
          cache.material->bind(cmd);
        }

        cmd.pushConstants(0, sizeof(glm::mat4), &t.worldMatrix);

        if (cache.texture) {
          cmd.bindTexture(0, *cache.texture);
        }

        if (cache.model) {
          for (auto &mesh : cache.model->meshes) {
            if (!mesh.isValid()) {
              continue;
            }
            cmd.bindVertexBuffer(*mesh.vertexBuffer);
            cmd.bindIndexBuffer(*mesh.indexBuffer);
            cmd.drawIndexed(mesh.indexCount);
          }
        }
      });
}

void ExampleGame::shutdown() {
  s_logger.info("Shutting down example game...");
  meshCaches_.clear();
  skyboxPipeline_.reset();
  skyboxTexture_.reset();
  skyboxVertexBuffer_.reset();
  skyboxIndexBuffer_.reset();
  pipeline_.reset();
}

extern "C" {

RAIDEN_EXPORT Raiden::Engine::IGamePlugin *raiden_create_plugin() {
  return new ExampleGame();
}

RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Engine::IGamePlugin *plugin) {
  delete plugin;
}
}
