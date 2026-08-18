#include "Game.hpp"
#include "Systems/TransformUpdateSystem.hpp"

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
#include <Raiden/Audio/IAudioDevice.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Physics/ContactEvent.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>
#include <Raiden/Scene/SceneSerializer.hpp>

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>

static const Raiden::Core::Logger s_logger("NewVisualDemo");

bool NewVisualDemo::init(Raiden::Renderer::IRenderDevice &device,
                         Raiden::Core::IVirtualFileSystem &vfs,
                         Raiden::Assets::IAssetManager &assets,
                         Raiden::Platform::IPlatform *platform,
                         Raiden::Audio::IAudioDevice *audio,
                         Raiden::Physics::IPhysicsSystem *physics) {
  platform_ = platform;
  device_ = &device;
  assets_ = &assets;
  vfs_ = &vfs;
  physics_ = physics;
  audio_ = audio;
  s_logger.info("Initializing New Visual Demo...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map.");
  }

  world_.registerComponent<Raiden::ECS::Name>("Name");
  world_.registerComponent<Raiden::ECS::Transform>("Transform");
  world_.registerComponent<Raiden::ECS::Camera>("Camera");
  world_.registerComponent<Raiden::ECS::DirectionalLight>("DirectionalLight");
  world_.registerComponent<Raiden::ECS::PointLight>("PointLight");
  world_.registerComponent<Raiden::ECS::AmbientLight>("AmbientLight");
  world_.registerComponent<Raiden::ECS::MeshRenderer>("MeshRenderer");
  world_.registerComponent<Raiden::ECS::Rigidbody>("Rigidbody");
  world_.registerComponent<Raiden::ECS::Collider>("Collider");

  // Load scene from JSON
  Raiden::Scene::SerializedScene scene;
  if (Raiden::Scene::loadJson(scene, vfs, "game://scenes/default.scene.json")) {
    Raiden::Scene::deserializeWorld(scene, world_);
    s_logger.info("Scene loaded: {} entities", scene.entities.size());
  } else {
    s_logger.warn("Failed to load scene, using empty world");
  }

  // Find the camera entity by name and set up CameraControllerSystem
  world_.view<Raiden::ECS::Name, Raiden::ECS::Camera>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Name &n, Raiden::ECS::Camera &) {
        if (n.value == "Player Camera") {
          camEntity_ = e;
        }
      });

  if (camEntity_ != Raiden::ECS::NullEntity) {
    auto &t = world_.get<Raiden::ECS::Transform>(camEntity_);
    camPos_ = t.translation;
    cameraSystem_ = &world_.addSystem<CameraControllerSystem>(
        actions_, platform_, audio_, camEntity_, camPos_, yaw_, pitch_,
        mouseCaptured_);
  }

  world_.addSystem<TransformUpdateSystem>();

  // Skybox setup
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
    s_logger.warn("Failed to load skybox cubemap");
  } else {
    if (device_->initIBL(skyboxTexture_)) {
      s_logger.info("IBL initialized from skybox");
    } else {
      s_logger.warn("IBL initialization failed");
    }
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

  // preload material cache for all mesh renderers
  s_logger.info("Preloading assets...");
  world_.view<Raiden::ECS::MeshRenderer>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::MeshRenderer &mr) {
        PbrTextures tex;
        tex.albedo = mr.texturePath;
        tex.normal = mr.normalMap;
        tex.metallicRoughness = mr.metallicRoughnessMap;
        tex.occlusion = mr.occlusionMap;
        getOrCreateCache(mr.meshPath, tex, mr.metallic, mr.roughness,
                         mr.baseColorFactor, mr.uvScale,
                         mr.triplanarMapping);
      });
  s_logger.info("Asset preload complete.");

  s_logger.info("New Visual Demo initialized: {} entities",
                world_.view<Raiden::ECS::Transform>().size());

  return true;
}

void NewVisualDemo::spawnBox() {
  if (boxCount_ >= kMaxBoxes) {
    return;
  }

  auto e = world_.create();
  std::string name = "Spawned Box " + std::to_string(boxCount_);
  world_.assign<Raiden::ECS::Name>(e, name.c_str());

  float x = camPos_.x + glm::linearRand(-2.0F, 2.0F);
  float z = camPos_.z + glm::linearRand(-2.0F, 2.0F);
  float y = camPos_.y + 1.0F;

  world_.assign<Raiden::ECS::Transform>(
      e, Raiden::ECS::Transform{
             .translation = {x, y, z},
             .scale = {0.35F, 0.35F, 0.35F},
         });

  float shade = glm::linearRand(0.5F, 1.0F);
  world_.assign<Raiden::ECS::MeshRenderer>(
      e, Raiden::ECS::MeshRenderer{
             .meshPath = "game://meshes/cube.glb",
             .texturePath = "game://textures/red_brick_diff_4k.ktx2",
             .normalMap = "game://textures/red_brick_nor_gl_4k.ktx2",
             .metallicRoughnessMap = "game://textures/red_brick_arm_4k.ktx2",
             .occlusionMap = "game://textures/red_brick_arm_4k.ktx2",
             .shader = "builtin://pbr",
             .baseColorFactor = {shade, shade, shade, 1.0F},
             .metallic = 0.0F,
             .roughness = glm::linearRand(0.3F, 0.9F),
         });

  if (physics_ != nullptr) {
    world_.assign<Raiden::ECS::Rigidbody>(
        e, Raiden::ECS::Rigidbody{
               .type = Raiden::ECS::Rigidbody::Type::Dynamic,
               .mass = glm::linearRand(0.5F, 3.0F),
               .friction = 0.5F,
               .restitution = 0.2F,
           });
    world_.assign<Raiden::ECS::Collider>(
        e, Raiden::ECS::Collider{
               .shape = Raiden::ECS::Collider::Shape::Box,
               .halfExtents = {0.175F, 0.175F, 0.175F},
           });
  }

  ++boxCount_;
}

void NewVisualDemo::resetBoxes() {
  std::vector<Raiden::ECS::Entity> toDestroy;
  world_.view<Raiden::ECS::Name, Raiden::ECS::Rigidbody>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Name &n,
          Raiden::ECS::Rigidbody &) {
        if (n.value.starts_with("Spawned Box")) {
          toDestroy.push_back(e);
        }
      });
  for (auto e : toDestroy) {
    world_.destroy(e);
  }
  boxCount_ = 0;
}

NewVisualDemo::MeshCache &NewVisualDemo::getOrCreateCache(
    const std::string &meshPath, const PbrTextures &textures,
    float metallic, float roughness,
    const glm::vec4 &baseColorFactor, const glm::vec2 &uvScale,
    bool triplanarMapping) {
  constexpr const char *kPbrShader = "builtin://pbr";
  auto key = meshPath + ":" + kPbrShader + ":" +
             textures.albedo + ":" +
             textures.normal + ":" + textures.metallicRoughness + ":" +
             textures.occlusion + ":" +
             std::to_string(metallic) + ":" + std::to_string(roughness) +
             ":" + std::to_string(uvScale.x) + ":" +
             std::to_string(uvScale.y) + ":" +
             std::to_string(static_cast<int>(triplanarMapping));

  auto it = meshCaches_.find(key);
  if (it != meshCaches_.end()) {
    return it->second;
  }

  MeshCache cache;
  cache.model = assets_->loadMesh(meshPath);
  if (!cache.model) {
    s_logger.error("Failed to load mesh '{}'", meshPath);
  }

  if (!textures.albedo.empty()) {
    cache.albedoTex = assets_->loadTextureSync(textures.albedo);
  }
  if (!textures.normal.empty()) {
    cache.normalTex = assets_->loadTextureSync(textures.normal);
  }
  if (!textures.metallicRoughness.empty()) {
    cache.mrTex = assets_->loadTextureSync(textures.metallicRoughness);
  }
  if (!textures.occlusion.empty()) {
    cache.aoTex = assets_->loadTextureSync(textures.occlusion);
  }

  Raiden::Renderer::MaterialDesc matDesc;
  matDesc.shader = kPbrShader;
  matDesc.baseColorFactor = baseColorFactor;
  matDesc.metallicFactor = metallic;
  matDesc.roughnessFactor = roughness;
  matDesc.uvScale = uvScale;
  matDesc.triplanarMapping = triplanarMapping;
  cache.material = device_->createMaterial(matDesc, cache.albedoTex,
                                           cache.normalTex, cache.mrTex,
                                           nullptr, cache.aoTex);

  auto [inserted, _] = meshCaches_.emplace(key, std::move(cache));
  return inserted->second;
}

void NewVisualDemo::update(float deltaTime,
                           const Raiden::Platform::InputState &input) {
  actions_.evaluate(input);

  if (const auto *sp = actions_.find("spawn_box");
      (sp != nullptr) && sp->justPressed) {
    spawnBox();
  }
  if (const auto *rs = actions_.find("reset_boxes");
      (rs != nullptr) && rs->justPressed) {
    resetBoxes();
  }
  cameraSystem_->setInput(input);
  world_.updateSystems(deltaTime);
}

void NewVisualDemo::render(Raiden::Renderer::ICommandBuffer &cmd) {
  // skybox
  if (skyboxPipeline_ && skyboxTexture_ && skyboxVertexBuffer_ &&
      skyboxIndexBuffer_) {
    cmd.bindPipeline(*skyboxPipeline_);
    cmd.bindTexture(0, *skyboxTexture_);
    cmd.bindVertexBuffer(*skyboxVertexBuffer_);
    cmd.bindIndexBuffer(*skyboxIndexBuffer_);
    cmd.drawIndexed(skyboxIndexCount_);
  }

  // all MeshRenderer entities
  world_.view<Raiden::ECS::Transform, Raiden::ECS::MeshRenderer>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::Transform &t,
          Raiden::ECS::MeshRenderer &mr) {
        PbrTextures tex;
        tex.albedo = mr.texturePath;
        tex.normal = mr.normalMap;
        tex.metallicRoughness = mr.metallicRoughnessMap;
        tex.occlusion = mr.occlusionMap;

        auto &cache =
            getOrCreateCache(mr.meshPath, tex, mr.metallic, mr.roughness,
                             mr.baseColorFactor, mr.uvScale,
                             mr.triplanarMapping);

        if (cache.material) {
          cache.material->bind(cmd);
        }

        cmd.pushConstants(0, sizeof(glm::mat4), &t.worldMatrix);

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

void NewVisualDemo::onDebugUI() {
  ImGui::SetNextWindowPos(ImVec2(10.0F, 300.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(280.0F, 220.0F), ImGuiCond_FirstUseEver);
  ImGui::Begin("New Visual Demo");

  ImGui::Text("Entities: %zu", world_.view<Raiden::ECS::Transform>().size());
  ImGui::Text("Spawned boxes: %d / %d", boxCount_, kMaxBoxes);
  ImGui::Text("Camera: (%.1f, %.1f, %.1f)", camPos_.x, camPos_.y, camPos_.z);

  if (ImGui::Button("Spawn Box (Space)")) {
    spawnBox();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset (R)")) {
    resetBoxes();
  }

  ImGui::Separator();
  ImGui::Text("Right-click: capture mouse");
  ImGui::Text("WASD: move  QE: up/down");
  ImGui::Text("Shift: sprint  Esc: quit");

  ImGui::Separator();
  ImGui::Text("Engine Features:");
  ImGui::BulletText("PBR (Cook-Torrance BRDF)");
  ImGui::BulletText("Directional light + shadows");
  ImGui::BulletText("Point lights (3)");
  ImGui::BulletText("Ambient + IBL");
  ImGui::BulletText("Skybox cubemap");
  ImGui::BulletText("Normal mapping");
  ImGui::BulletText("Jolt Physics");
  ImGui::BulletText("ECS (archetype)");
  ImGui::BulletText("3D audio (OpenAL)");
  ImGui::BulletText("Debug UI (ImGui)");
  ImGui::BulletText("Action map input");

  ImGui::End();
}

void NewVisualDemo::shutdown() {
  s_logger.info("Shutting down New Visual Demo...");
  meshCaches_.clear();
  skyboxPipeline_.reset();
  skyboxTexture_.reset();
  skyboxVertexBuffer_.reset();
  skyboxIndexBuffer_.reset();
}

extern "C" {

RAIDEN_EXPORT Raiden::Engine::IGamePlugin *raiden_create_plugin() {
  return new NewVisualDemo();
}

RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Engine::IGamePlugin *plugin) {
  delete plugin;
}
}
