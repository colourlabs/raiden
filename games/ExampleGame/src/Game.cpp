#include "Game.hpp"

#include <Raiden/Assets/IAssetManager.hpp>
#include <Raiden/Core/ConVar.hpp>
#include <Raiden/Core/PluginABI.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Collider.hpp>
#include <Raiden/ECS/Light.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Rigidbody.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Physics/ContactEvent.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

static const Raiden::Core::Logger s_logger("ExampleGame");

bool ExampleGame::init(Raiden::Renderer::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Assets::IAssetManager &assets,
                       Raiden::Platform::IPlatform *platform,
                       Raiden::Audio::IAudioDevice *audio,
                       Raiden::Physics::IPhysicsSystem *physics) {
  (void)audio;

  platform_ = platform;
  device_ = &device;
  assets_ = &assets;
  physics_ = physics;
  s_logger.info("Initializing example game...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map, continuing without actions.");
  }

  // camera
  camEntity_ = world_.create();

  world_.assign<Raiden::ECS::Name>(camEntity_, "Main Camera");
  world_.assign<Raiden::ECS::Camera>(camEntity_);
  world_.assign<Raiden::ECS::Transform>(camEntity_,
                                        Raiden::ECS::Transform{
                                            .translation = {0.0F, 0.0F, 3.0F},
                                        });

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);
  cam.setLookAt({0.0F, 0.0F, 3.0F}, {0.0F, 0.0F, 0.0F});
  cam.setPerspective(45.0F, 1.0F, 0.1F, 100.0F);

  // directional light with shadows
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Sun");
    world_.assign<Raiden::ECS::DirectionalLight>(
        e, Raiden::ECS::DirectionalLight{
               .direction = {0.3F, -1.0F, 0.5F},
               .color = {1.0F, 0.98F, 0.92F},
               .intensity = 2.0F,
               .castShadows = true,
               .shadowNear = 0.1F,
               .shadowFar = 50.0F,
               .shadowSize = 20.0F,
           });
  }

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
    world_.assign<Raiden::ECS::Transform>(e,
                                          Raiden::ECS::Transform{
                                              .translation = {0.0F, 0.0F, 0.0F},
                                              .scale = {0.5F, 0.5F, 0.5F},
                                          });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
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

    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/cube.glb",
               .texturePath = "",
               .shader = "builtin://pbr",
               .baseColorFactor = p.color,
               .metallic = p.metallic,
               .roughness = p.roughness,
           });
  }

  // physics demo: static floor
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Floor");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {0.0F, -1.5F, 0.0F},
               .scale = {10.0F, 0.1F, 10.0F},
           });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/cube.glb",
               .texturePath = "game://textures/checkerboard.ktx2",
           });
    world_.assign<Raiden::ECS::Rigidbody>(
        e, Raiden::ECS::Rigidbody{
               .type = Raiden::ECS::Rigidbody::Type::Static,
           });
    world_.assign<Raiden::ECS::Collider>(
        e, Raiden::ECS::Collider{
               .shape = Raiden::ECS::Collider::Shape::Box,
               .halfExtents = {5.0F, 0.05F, 5.0F},
           });
  }

  // physics demo: dynamic falling cubes
  if (physics_ != nullptr) {
    struct FallPreset {
      const char *name;
      glm::vec3 position;
      float mass;
    };

    std::array<FallPreset, 3> falls = {{
        {.name = "Falling Cube 1",
         .position = {0.0F, 2.0F, 0.0F},
         .mass = 1.0F},
        {.name = "Falling Cube 2",
         .position = {0.5F, 3.0F, 0.0F},
         .mass = 2.0F},
        {.name = "Falling Cube 3",
         .position = {-0.5F, 4.0F, 0.0F},
         .mass = 0.5F},
    }};

    for (auto &f : falls) {
      auto e = world_.create();
      world_.assign<Raiden::ECS::Name>(e, f.name);
      world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
                                                   .translation = f.position,
                                                   .scale = {0.3F, 0.3F, 0.3F},
                                               });
      world_.assign<Raiden::ECS::MeshRenderer>(
          e, Raiden::ECS::MeshRenderer{
                 .meshPath = "game://meshes/cube.glb",
                 .texturePath = "",
                 .shader = "builtin://pbr",
                 .baseColorFactor = {0.9F, 0.4F, 0.2F, 1.0F},
                 .metallic = 0.0F,
                 .roughness = 0.6F,
             });
      world_.assign<Raiden::ECS::Rigidbody>(
          e, Raiden::ECS::Rigidbody{
                 .type = Raiden::ECS::Rigidbody::Type::Dynamic,
                 .mass = f.mass,
             });
      world_.assign<Raiden::ECS::Collider>(
          e, Raiden::ECS::Collider{
                 .shape = Raiden::ECS::Collider::Shape::Box,
                 .halfExtents = {0.15F, 0.15F, 0.15F},
             });
    }
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
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 3, 7, 4, 3, 4, 0,
      1, 5, 6, 1, 6, 2, 3, 2, 6, 3, 6, 7, 0, 4, 5, 0, 5, 1,
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

const char *ExampleGame::findName(uint32_t bodyId) {
  const char *result = "Unknown";
  world_.view<Raiden::ECS::Name, Raiden::ECS::Rigidbody>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::Name &n,
          Raiden::ECS::Rigidbody &rb) {
        if (rb.bodyId == bodyId) {
          result = n.value.c_str();
        }
      });
  return result;
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
    float const sensitivity =
        Raiden::Core::convars().getFloat("mouse_sensitivity", 0.002F);
    yaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  float speed =
      Raiden::Core::convars().getFloat("camera_speed", 3.0F) * deltaTime;
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

  // log collisions
  if (physics_ != nullptr) {
    for (const auto &contact : physics_->contacts()) {
      const auto *nameA = findName(contact.bodyIdA);
      const auto *nameB = findName(contact.bodyIdB);

      if (contact.type == Raiden::Physics::ContactEvent::Type::Added) {
        s_logger.info("Collision: {} hit {}", nameA, nameB);
      }
    }
  }

  rotation_ += deltaTime * 45.0F;

  // update rotating objects
  world_.view<Raiden::ECS::Name, Raiden::ECS::Transform>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::Name &n,
          Raiden::ECS::Transform &t) {
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
        auto &cache =
            getOrCreateCache(mr.meshPath, mr.texturePath, mr.shader,
                             mr.metallic, mr.roughness, mr.baseColorFactor);

        if (cache.material) {
          cache.material->bind(cmd);
        }

        cmd.pushConstants(0, sizeof(glm::mat4), &t.worldMatrix);

        if (!cache.material && cache.texture) {
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
