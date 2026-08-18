#include "VisualDemo.hpp"

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
#include <Raiden/Physics/IPhysicsSystem.hpp>
#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <imgui.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>

static const Raiden::Core::Logger s_logger("VisualDemo");

static const std::string kWoodAlbedo = "game://textures/oak-wood-bare_albedo.ktx2";
static const std::string kWoodNormal = "game://textures/oak-wood-bare_normal-dx.ktx2";
static const std::string kWoodRoughness = "game://textures/oak-wood-bare_roughness.ktx2";
static const std::string kWoodAo = "game://textures/oak-wood-bare_ao.ktx2";

static const std::string kGrassAlbedo = "game://textures/grass_albedo.ktx2";
static const std::string kGrassNormal = "game://textures/grass_normal.ktx2";
static const std::string kGrassRoughness = "game://textures/grass_roughness.ktx2";
static const std::string kGrassAo = "game://textures/grass_ao.ktx2";

bool VisualDemo::init(Raiden::Renderer::IRenderDevice &device,
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
  s_logger.info("Initializing Visual Demos...");

  if (!actions_.loadFromFile(vfs, "game://config/actions.toml")) {
    s_logger.warn("Failed to load action map.");
  }

  // register component names for the entity inspector
  world_.registerComponent<Raiden::ECS::Name>("Name");
  world_.registerComponent<Raiden::ECS::Transform>("Transform");
  world_.registerComponent<Raiden::ECS::Camera>("Camera");
  world_.registerComponent<Raiden::ECS::DirectionalLight>("DirectionalLight");
  world_.registerComponent<Raiden::ECS::PointLight>("PointLight");
  world_.registerComponent<Raiden::ECS::AmbientLight>("AmbientLight");
  world_.registerComponent<Raiden::ECS::MeshRenderer>("MeshRenderer");
  world_.registerComponent<Raiden::ECS::Rigidbody>("Rigidbody");
  world_.registerComponent<Raiden::ECS::Collider>("Collider");

  // --- camera ---
  camEntity_ = world_.create();
  world_.assign<Raiden::ECS::Name>(camEntity_, "Camera");
  world_.assign<Raiden::ECS::Camera>(camEntity_);
  world_.assign<Raiden::ECS::Transform>(camEntity_, Raiden::ECS::Transform{
                                                        .translation = camPos_,
                                                    });

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);
  cam.setPerspective(60.0F, 16.0F / 9.0F, 0.1F, 200.0F);

  // directional light (sun)
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Sun");
    world_.assign<Raiden::ECS::DirectionalLight>(
        e, Raiden::ECS::DirectionalLight{
               .direction = {-0.4F, -0.8F, 0.3F},
               .color = {1.0F, 0.95F, 0.85F},
               .intensity = 1.0F,
               .castShadows = true,
               .shadowNear = 0.1F,
               .shadowFar = 100.0F,
               .shadowSize = 55.0F,
               .shadowMapResolution = 4096,
           });
  }

  // ambient / fill light to prevent pitch-black shadows
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Ambient");
    world_.assign<Raiden::ECS::AmbientLight>(
        e, Raiden::ECS::AmbientLight{
               .skyColor = {0.50F, 0.55F, 0.65F},
               .skyIntensity = 1.5F,
               .groundColor = {0.15F, 0.12F, 0.10F},
               .useIBL = true,
           });
  }

  // point lights for warm/cool contrast
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Warm Point Light");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {-4.0F, 2.0F, -2.0F},
           });
    world_.assign<Raiden::ECS::PointLight>(e,
                                            Raiden::ECS::PointLight{
                                                .position = {-4.0F, 2.0F, -2.0F},
                                                .color = {1.0F, 0.6F, 0.2F},
                                                .intensity = 5.0F,
                                                .range = 35.0F,
                                            });
  }

  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Cool Point Light");
    world_.assign<Raiden::ECS::Transform>(e,
                                          Raiden::ECS::Transform{
                                              .translation = {4.0F, 3.0F, 3.0F},
                                          });
    world_.assign<Raiden::ECS::PointLight>(e,
                                            Raiden::ECS::PointLight{
                                                .position = {4.0F, 3.0F, 3.0F},
                                                .color = {0.3F, 0.5F, 1.0F},
                                                .intensity = 4.0F,
                                                .range = 35.0F,
                                            });
  }

  // --- main pipeline ---
  pipeline_ = device.createPipeline(Raiden::Renderer::PipelineDesc{
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
  });
  if (!pipeline_) {
    s_logger.error("Failed to create pipeline");
    return false;
  }

  //  GRASSLAND

  // large ground plane (green grass)
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Grassland");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {0.0F, -0.5F, 0.0F},
               .scale = {50.0F, 0.1F, 50.0F},
           });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/plane.glb",
               .texturePath = kGrassAlbedo,
               .normalMap = kGrassNormal,
               .occlusionMap = kGrassAo,
               .shader = "builtin://pbr",
               .baseColorFactor = {1.0F, 1.0F, 1.0F, 1.0F},
               .metallic = 0.0F,
               .roughness = 0.95F,
           });
    world_.assign<Raiden::ECS::Rigidbody>(
        e,
        Raiden::ECS::Rigidbody{.type = Raiden::ECS::Rigidbody::Type::Static});
    world_.assign<Raiden::ECS::Collider>(
        e, Raiden::ECS::Collider{
               .shape = Raiden::ECS::Collider::Shape::Box,
               .halfExtents = {25.0F, 0.05F, 25.0F},
           });
  }

  // dirt layer under grass
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Dirt");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {0.0F, -0.8F, 0.0F},
               .scale = {50.0F, 0.3F, 50.0F},
           });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/cube.glb",
               .texturePath = "",
               .normalMap = "",
               .occlusionMap = "",
               .shader = "builtin://pbr",
               .baseColorFactor = {0.45F, 0.30F, 0.15F, 1.0F},
               .metallic = 0.0F,
               .roughness = 1.0F,
           });
  }

  // grass tufts scattered on the ground
  {
    struct Tuft {
      glm::vec3 pos;
      glm::vec3 scale;
      float shade;
    };

    std::vector<Tuft> tufts;
    for (int i = 0; i < 40; ++i) {
      float x = glm::linearRand(-18.0F, 18.0F);
      float z = glm::linearRand(-18.0F, 18.0F);
      float s = glm::linearRand(0.15F, 0.4F);
      float shade = glm::linearRand(0.3F, 0.7F);
      float sy = s * 1.5F;
      float y = -0.5F + (sy * 0.5F);
      tufts.push_back({{x, y, z}, {s, sy, s}, shade});
    }

    for (auto &t : tufts) {
      auto e = world_.create();
      world_.assign<Raiden::ECS::Name>(e, "Grass Tuft");
      world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
                                                   .translation = t.pos,
                                                   .scale = t.scale,
                                               });
      world_.assign<Raiden::ECS::MeshRenderer>(
          e, Raiden::ECS::MeshRenderer{
                 .meshPath = "game://meshes/cube.glb",
                 .texturePath = "",
                 .normalMap = "",
                 .occlusionMap = "",
                 .shader = "builtin://pbr",
                 .baseColorFactor = {0.15F, t.shade, 0.10F, 1.0F},
                 .metallic = 0.0F,
                 .roughness = 0.9F,
             });
    }
  }

  //  CRATE BOXES

  // wooden floor platform
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Wooden Platform");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {0.0F, -0.44F, 0.0F},
               .scale = {6.0F, 0.02F, 6.0F},
           });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/cube.glb",
               .texturePath = kWoodAlbedo,
               .normalMap = kWoodNormal,
               .occlusionMap = kWoodAo,
               .shader = "builtin://pbr",
               .baseColorFactor = {1.0F, 1.0F, 1.0F, 1.0F},
               .metallic = 0.0F,
               .roughness = 0.75F,
           });
    world_.assign<Raiden::ECS::Rigidbody>(
        e,
        Raiden::ECS::Rigidbody{.type = Raiden::ECS::Rigidbody::Type::Static});
    world_.assign<Raiden::ECS::Collider>(
        e, Raiden::ECS::Collider{
               .shape = Raiden::ECS::Collider::Shape::Box,
               .halfExtents = {3.0F, 0.01F, 3.0F},
           });
  }

  // scattered crate boxes on the ground
  struct CratePreset {
    const char *name;
    glm::vec3 position;
    float mass;
    float roughness;
    float shade;
  };

  std::array<CratePreset, 12> crates = {{
      {.name = "Crate A",
       .position = {-2.0F, 0.2F, -1.0F},
       .mass = 1.0F,
       .roughness = 0.8F,
       .shade = 0.9F},
      {.name = "Crate B",
       .position = {-1.0F, 0.2F, 1.5F},
       .mass = 1.5F,
       .roughness = 0.7F,
       .shade = 0.85F},
      {.name = "Crate C",
       .position = {0.5F, 0.2F, -2.0F},
       .mass = 2.0F,
       .roughness = 0.85F,
       .shade = 0.8F},
      {.name = "Crate D",
       .position = {1.5F, 0.2F, 0.5F},
       .mass = 0.8F,
       .roughness = 0.75F,
       .shade = 0.75F},
      {.name = "Crate E",
       .position = {2.5F, 0.2F, -1.5F},
       .mass = 1.2F,
       .roughness = 0.9F,
       .shade = 0.7F},
      {.name = "Crate F",
       .position = {-2.5F, 0.2F, 2.0F},
       .mass = 1.0F,
       .roughness = 0.8F,
       .shade = 0.65F},
      {.name = "Crate G",
       .position = {0.0F, 0.2F, 2.5F},
       .mass = 1.8F,
       .roughness = 0.85F,
       .shade = 0.95F},
      {.name = "Crate H",
       .position = {2.0F, 0.2F, 2.5F},
       .mass = 0.7F,
       .roughness = 0.7F,
       .shade = 0.88F},
      {.name = "Crate I",
       .position = {-1.5F, 0.2F, -2.5F},
       .mass = 1.3F,
       .roughness = 0.75F,
       .shade = 0.82F},
      {.name = "Crate J",
       .position = {1.0F, 0.2F, -2.5F},
       .mass = 1.1F,
       .roughness = 0.8F,
       .shade = 0.72F},
      {.name = "Crate K",
       .position = {3.0F, 0.2F, 1.0F},
       .mass = 1.6F,
       .roughness = 0.9F,
       .shade = 0.6F},
      {.name = "Crate L",
       .position = {-3.0F, 0.2F, -0.5F},
       .mass = 0.9F,
       .roughness = 0.85F,
       .shade = 0.78F},
  }};

  for (auto &c : crates) {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, c.name);
    world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
                                                 .translation = c.position,
                                                 .scale = {0.5F, 0.5F, 0.5F},
                                             });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/cube.glb",
               .texturePath = kWoodAlbedo,
               .normalMap = kWoodNormal,
               .occlusionMap = kWoodAo,
               .shader = "builtin://pbr",
               .baseColorFactor = {c.shade, c.shade, c.shade, 1.0F},
               .metallic = 0.0F,
               .roughness = c.roughness,
           });

    if (physics_ != nullptr) {
      world_.assign<Raiden::ECS::Rigidbody>(
          e, Raiden::ECS::Rigidbody{
                 .type = Raiden::ECS::Rigidbody::Type::Dynamic,
                 .mass = c.mass,
                 .friction = 0.6F,
                 .restitution = 0.15F,
             });
      world_.assign<Raiden::ECS::Collider>(
          e, Raiden::ECS::Collider{
                 .shape = Raiden::ECS::Collider::Shape::Box,
                 .halfExtents = {0.25F, 0.25F, 0.25F},
             });
    }
  }

  // stacked tower of crates
  {
    struct StackPos {
      glm::vec3 pos;
      int row;
    };
    std::vector<StackPos> stack;
    for (int y = 0; y < 4; ++y) {
      int count = 4 - y;
      float offset = (static_cast<float>(count) - 1.0F) * 0.3F;
      for (int x = 0; x < count; ++x) {
        stack.push_back({{-5.0F + offset + (static_cast<float>(x) * 0.6F),
                          -0.19F + (static_cast<float>(y) * 0.52F), -3.0F},
                         static_cast<int>(static_cast<float>(x))});
      }
    }

    for (size_t i = 0; i < stack.size(); ++i) {
      auto &s = stack[i];
      auto e = world_.create();
      std::string name = "Stack Box " + std::to_string(i);
      world_.assign<Raiden::ECS::Name>(e, name.c_str());
      world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
                                                   .translation = s.pos,
                                                   .scale = {0.5F, 0.5F, 0.5F},
                                               });

      float shade = 0.65F + (0.15F * static_cast<float>(s.row));
      world_.assign<Raiden::ECS::MeshRenderer>(
          e, Raiden::ECS::MeshRenderer{
                 .meshPath = "game://meshes/cube.glb",
                 .texturePath = kWoodAlbedo,
                 .normalMap = kWoodNormal,
                 .occlusionMap = kWoodAo,
                 .shader = "builtin://pbr",
                 .baseColorFactor = {shade, shade, shade, 1.0F},
                 .metallic = 0.0F,
                 .roughness = 0.85F,
             });

      if (physics_ != nullptr) {
        world_.assign<Raiden::ECS::Rigidbody>(
            e, Raiden::ECS::Rigidbody{
                   .type = Raiden::ECS::Rigidbody::Type::Dynamic,
                   .mass = 1.0F,
                   .friction = 0.6F,
                   .restitution = 0.1F,
               });
        world_.assign<Raiden::ECS::Collider>(
            e, Raiden::ECS::Collider{
                   .shape = Raiden::ECS::Collider::Shape::Box,
                   .halfExtents = {0.25F, 0.25F, 0.25F},
               });
      }
    }
  }

  // cottage (loaded from FBX if available, else glTF)
  {
    auto e = world_.create();
    world_.assign<Raiden::ECS::Name>(e, "Cottage");
    world_.assign<Raiden::ECS::Transform>(
        e, Raiden::ECS::Transform{
               .translation = {8.0F, -0.5F, -5.0F},
               .scale = {0.01F, 0.01F, 0.01F},
           });
    world_.assign<Raiden::ECS::MeshRenderer>(
        e, Raiden::ECS::MeshRenderer{
               .meshPath = "game://meshes/Cottage.fbx",
               .texturePath = kWoodAlbedo,
               .normalMap = kWoodNormal,
               .occlusionMap = kWoodAo,
               .shader = "builtin://pbr",
               .baseColorFactor = {0.9F, 0.85F, 0.75F, 1.0F},
               .metallic = 0.0F,
               .roughness = 0.9F,
           });
  }

  // metal showcase spheres
  {
    for (int i = 0; i < 5; ++i) {
      float x = (static_cast<float>(i) * 1.5F) - 3.0F;
      auto e = world_.create();
      std::string name = "Metal Sphere " + std::to_string(i);
      world_.assign<Raiden::ECS::Name>(e, name.c_str());
      world_.assign<Raiden::ECS::Transform>(e,
                                            Raiden::ECS::Transform{
                                                .translation = {x, 0.5F, 5.0F},
                                                .scale = {0.5F, 0.5F, 0.5F},
                                            });
      float roughness = 0.1F + (static_cast<float>(i) * 0.2F);
      world_.assign<Raiden::ECS::MeshRenderer>(
          e, Raiden::ECS::MeshRenderer{
                 .meshPath = "game://meshes/cube.glb",
                 .texturePath = "",
                 .normalMap = "",
                 .occlusionMap = "",
                 .shader = "builtin://pbr",
                 .baseColorFactor = {0.9F, 0.85F, 0.8F, 1.0F},
                 .metallic = 1.0F,
                 .roughness = roughness,
             });
    }
  }

  // SKYBOX
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
      s_logger.warn("IBL initialization failed, falling back to hemisphere ambient");
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

  s_logger.info("Visual Demos initialized: {} entities in scene",
                world_.view<Raiden::ECS::Transform>().size());

  s_logger.info("Preloading assets...");
  world_.view<Raiden::ECS::MeshRenderer>().each(
      [&](Raiden::ECS::Entity, Raiden::ECS::MeshRenderer &mr) {
        PbrTextures tex;
        tex.albedo = mr.texturePath;
        tex.normal = mr.normalMap;
        tex.occlusion = mr.occlusionMap;
        getOrCreateCache(mr.meshPath, tex, mr.shader, mr.metallic,
                         mr.roughness, mr.baseColorFactor);
      });
  s_logger.info("Asset preload complete.");

  return true;
}

void VisualDemo::spawnBox() {
  if (boxCount_ >= kMaxBoxes) {
    return;
  }

  auto e = world_.create();
  std::string name = "Spawned Box " + std::to_string(boxCount_);
  world_.assign<Raiden::ECS::Name>(e, name.c_str());

  float x = glm::linearRand(-3.0F, 3.0F);
  float z = glm::linearRand(-3.0F, 3.0F);
  float y = glm::linearRand(6.0F, 10.0F);

  world_.assign<Raiden::ECS::Transform>(e, Raiden::ECS::Transform{
                                               .translation = {x, y, z},
                                               .scale = {0.4F, 0.4F, 0.4F},
                                           });

  float shade = glm::linearRand(0.5F, 1.0F);
  world_.assign<Raiden::ECS::MeshRenderer>(
      e,
      Raiden::ECS::MeshRenderer{
          .meshPath = "game://meshes/cube.glb",
          .texturePath = kWoodAlbedo,
          .normalMap = kWoodNormal,
          .occlusionMap = kWoodAo,
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
               .halfExtents = {0.2F, 0.2F, 0.2F},
           });
  }

  ++boxCount_;
  s_logger.info("Spawned box: {} ({}, {}, {})", name, x, y, z);
}

void VisualDemo::resetBoxes() {
  std::vector<Raiden::ECS::Entity> toDestroy;
  world_.view<Raiden::ECS::Name, Raiden::ECS::Rigidbody>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Name &n,
          Raiden::ECS::Rigidbody & /*rb*/) {
        if (n.value.find("Spawned Box") == 0) {
          toDestroy.push_back(e);
        }
      });

  for (auto e : toDestroy) {
    world_.destroy(e);
  }
  boxCount_ = 0;
  s_logger.info("Reset all spawned boxes");
}

VisualDemo::MeshCache &VisualDemo::getOrCreateCache(
    const std::string &meshPath, const PbrTextures &textures,
    const std::string &shader, float metallic, float roughness,
    const glm::vec4 &baseColorFactor) {
  auto key = meshPath + ":" + shader + ":" + textures.albedo + ":" +
             textures.normal + ":" + textures.occlusion + ":" +
             std::to_string(metallic) + ":" + std::to_string(roughness);

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
  if (!textures.occlusion.empty()) {
    cache.aoTex = assets_->loadTextureSync(textures.occlusion);
  }

  Raiden::Renderer::MaterialDesc matDesc;
  matDesc.shader = shader;
  matDesc.baseColorFactor = baseColorFactor;
  matDesc.metallicFactor = metallic;
  matDesc.roughnessFactor = roughness;
  cache.material = device_->createMaterial(matDesc, cache.albedoTex,
                                           cache.normalTex, nullptr,
                                           nullptr, cache.aoTex);

  auto [inserted, _] = meshCaches_.emplace(key, std::move(cache));
  return inserted->second;
}

void VisualDemo::update(float deltaTime,
                        const Raiden::Platform::InputState &input) {
  actions_.evaluate(input);

  // mouse capture toggle (right-click)
  static bool prevRmb = false;
  if (input.mouseButtons[2] && !prevRmb) {
    mouseCaptured_ = !mouseCaptured_;
    platform_->setRelativeMouseMode(mouseCaptured_);
  }
  prevRmb = input.mouseButtons[2];

  // spawn / reset boxes
  if (const auto *spawn = actions_.find("spawn_box");
      (spawn != nullptr) && spawn->justPressed) {
    spawnBox();
  }
  if (const auto *reset = actions_.find("reset_boxes");
      (reset != nullptr) && reset->justPressed) {
    resetBoxes();
  }

  // camera look
  if (mouseCaptured_) {
    float sensitivity =
        Raiden::Core::convars().getFloat("mouse_sensitivity", 0.002F);
    yaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  // camera movement
  float speed =
      Raiden::Core::convars().getFloat("camera_speed", 5.0F) * deltaTime;
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
    camPos_ += forward * speed;
  }
  if (const auto *mv = actions_.find("move_back");
      (mv != nullptr) && mv->pressed) {
    camPos_ -= forward * speed;
  }
  if (const auto *mv = actions_.find("move_left");
      (mv != nullptr) && mv->pressed) {
    camPos_ -= right * speed;
  }
  if (const auto *mv = actions_.find("move_right");
      (mv != nullptr) && mv->pressed) {
    camPos_ += right * speed;
  }
  if (const auto *mv = actions_.find("move_up");
      (mv != nullptr) && mv->pressed) {
    camPos_ += up * speed;
  }
  if (const auto *mv = actions_.find("move_down");
      (mv != nullptr) && mv->pressed) {
    camPos_ -= up * speed;
  }

  auto &cam = world_.get<Raiden::ECS::Camera>(camEntity_);
  cam.view = glm::lookAt(camPos_, camPos_ + forward, {0.0F, 1.0F, 0.0F});
  cam.setPerspective(60.0F, 16.0F / 9.0F, 0.1F, 200.0F);

  Raiden::ECS::updateTransforms(world_);
}

void VisualDemo::render(Raiden::Renderer::ICommandBuffer &cmd) {
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
        PbrTextures tex;
        tex.albedo = mr.texturePath;
        tex.normal = mr.normalMap;
        tex.occlusion = mr.occlusionMap;

        auto &cache =
            getOrCreateCache(mr.meshPath, tex, mr.shader,
                             mr.metallic, mr.roughness, mr.baseColorFactor);

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

void VisualDemo::onDebugUI() {
  ImGui::SetNextWindowPos(ImVec2(10.0F, 300.0F), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(250.0F, 120.0F), ImGuiCond_FirstUseEver);
  ImGui::Begin("Visual Demos");

  ImGui::Text("Entities: %zu", world_.view<Raiden::ECS::Transform>().size());
  ImGui::Text("Spawned boxes: %d / %d", boxCount_, kMaxBoxes);

  if (ImGui::Button("Spawn Box (Space)")) {
    spawnBox();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset (R)")) {
    resetBoxes();
  }

  ImGui::Separator();
  ImGui::Text("Right-click: capture mouse");
  ImGui::Text("WASD: move, QE: up/down");

  ImGui::End();
}

void VisualDemo::shutdown() {
  s_logger.info("Shutting down Visual Demos...");
  meshCaches_.clear();
  skyboxPipeline_.reset();
  skyboxTexture_.reset();
  skyboxVertexBuffer_.reset();
  skyboxIndexBuffer_.reset();
  pipeline_.reset();
}

extern "C" {

RAIDEN_EXPORT Raiden::Engine::IGamePlugin *raiden_create_plugin() {
  return new VisualDemo();
}

RAIDEN_EXPORT void raiden_destroy_plugin(Raiden::Engine::IGamePlugin *plugin) {
  delete plugin;
}
}
