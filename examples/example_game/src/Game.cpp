#include "Game.hpp"

#include <RaidenEngineCore/ECS/Transform.hpp>
#include <RaidenEngineCore/ECS/World.hpp>
#include <RaidenEngineCore/Logger.hpp>
#include <RaidenEngineCore/Renderer/ICommandBuffer.hpp>

#include <array>
#include <cstdint>
#include <vector>

// TODO: the game code shouldn't care about how stuff is being rendered by
// pushing stuff on the (abstracted) render pipeline, unless if the dev wants that flexiblity
// this like this because there literally is no ECS system as of now. (never mind, but its not finished)

// the aim is so developers have a low-level way to interact with the engine with C++
// and then maybe a high-level scripting language that allows people to quickly implement gameplay features

static const Raiden::Core::Logger s_logger("ExampleGame");

// stolen from somewhere for testing
static std::vector<uint8_t> makeCheckerboard(uint32_t w, uint32_t h,
                                             uint32_t tileSize) {
  std::vector<uint8_t> pixels(w * h * 4);
  for (uint32_t y = 0; y < h; ++y) {
    for (uint32_t x = 0; x < w; ++x) {
      bool light = ((x / tileSize) + (y / tileSize)) % 2 == 0;
      uint8_t c = light ? 0xF0 : 0x30;
      size_t i = (y * w + x) * 4;
      pixels[i + 0] = c;
      pixels[i + 1] = c;
      pixels[i + 2] = c;
      pixels[i + 3] = 0xFF;
    }
  }
  return pixels;
}

static struct EcsTestComp1 { int x; } s_testTag;
static struct EcsTestComp2 { float y; } s_testTag2;

bool ExampleGame::init(Raiden::Core::IRenderDevice &device,
                       Raiden::Core::IVirtualFileSystem &vfs) {
  s_logger.info("Initializing example game...");

  // ECS smoke test
  {
    Raiden::Core::World world;

    // Transform hierarchy test
    auto root = world.create();
    world.assign<Raiden::Core::Transform>(root);
    auto &rt = world.get<Raiden::Core::Transform>(root);
    rt.translation = {10.f, 0.f, 0.f};

    auto child = world.create();
    world.assign<Raiden::Core::Transform>(child);
    auto &ct = world.get<Raiden::Core::Transform>(child);
    ct.translation = {1.f, 0.f, 0.f};
    ct.parent = root;

    Raiden::Core::updateTransforms(world);
    auto &cwt = world.get<Raiden::Core::Transform>(child);
    
    s_logger.info("Transform test: child world X = {}", cwt.worldMatrix[3][0]);

    // basic ECS test
    auto a = world.create();
    auto b = world.create();
    
    world.assign<EcsTestComp1>(a, 42);
    world.assign<EcsTestComp2>(a, 3.14f);
    world.assign<EcsTestComp1>(b, 99);
    
    int sum = 0;

    world.view<EcsTestComp1>().each([&](EcsTestComp1 &c) { sum += c.x; });
    world.view<EcsTestComp1, EcsTestComp2>().each(
        [&](EcsTestComp1 &c, EcsTestComp2 &d) {
          s_logger.info("ECS test: ({}, {})", c.x, d.y);
        });
    s_logger.info("ECS smoke test passed (sum={})", sum);
  }

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
                      {2, Raiden::Core::Format::R32G32_Float,
                       offsetof(Vertex, uv)},
                  },
          },
      .depthTestEnable = true,
  };

  pipeline_ = device.createPipeline(pipelineDesc);
  if (!pipeline_) {
    s_logger.error("Failed to create pipeline");
    return false;
  }

  // quad vertices (pos, color, uv)
  std::array<Vertex, 4> vertices = {{
      Vertex{glm::vec2{-0.7f, -0.7f}, glm::vec3{1.0f, 0.0f, 0.0f},
             glm::vec2{0.0f, 1.0f}},
      Vertex{glm::vec2{0.7f, -0.7f}, glm::vec3{0.0f, 1.0f, 0.0f},
             glm::vec2{1.0f, 1.0f}},
      Vertex{glm::vec2{0.7f, 0.7f}, glm::vec3{0.0f, 0.0f, 1.0f},
             glm::vec2{1.0f, 0.0f}},
      Vertex{glm::vec2{-0.7f, 0.7f}, glm::vec3{1.0f, 1.0f, 0.0f},
             glm::vec2{0.0f, 0.0f}},
  }};

  vertexBuffer_ =
      device.createBuffer({sizeof(vertices), Raiden::Core::BufferUsage::Vertex,
                           Raiden::Core::MemoryAccess::CpuToGpu});
  if (!vertexBuffer_) {
    s_logger.error("Failed to create vertex buffer");
    return false;
  }
  vertexBuffer_->upload(vertices.data(), sizeof(vertices));

  // index buffer (two triangles for the quad)
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

  // procedural checkerboard texture
  auto pixels = makeCheckerboard(16, 16, 4);
  texture_ =
      device.createTexture({16, 16, Raiden::Core::Format::R8G8B8A8_SRGB});
  if (!texture_) {
    s_logger.error("Failed to create texture");
    return false;
  }
  texture_->upload(pixels.data(), pixels.size());

  s_logger.info("Example game initialized.");
  return true;
}

void ExampleGame::update(float /* deltaTime */,
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
