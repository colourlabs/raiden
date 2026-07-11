#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <gtest/gtest.h>

using namespace Raiden::ECS;

struct Health {
  int hp = 100;
};

struct Velocity {
  float x = 0.0F;
  float y = 0.0F;
};

class WorldTest : public testing::Test {
protected:
  void SetUp() override {
    Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);
  }
};

TEST_F(WorldTest, CreateEntity) {
  World world;
  Entity e = world.create();
  EXPECT_NE(e, NullEntity);
  EXPECT_EQ(e.index, 0);
  EXPECT_EQ(e.generation, 1);
}

TEST_F(WorldTest, CreateMultipleEntities) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  EXPECT_NE(a, b);
  EXPECT_EQ(a.index, 0);
  EXPECT_EQ(b.index, 1);
}

TEST_F(WorldTest, DestroyAndRecycle) {
  World world;
  Entity e = world.create();
  EXPECT_EQ(e.generation, 1);
  world.destroy(e);
  // generation was bumped to 2 by destroy

  Entity recycled = world.create();
  EXPECT_EQ(recycled.index, 0);
  EXPECT_EQ(recycled.generation, 3); // destroy bumps to 2, create bumps to 3
}

TEST_F(WorldTest, DestroyInvalidEntityNoCrash) {
  World world;
  world.create();
  // stale generation should be silently ignored
  world.destroy(Entity{.index = 0, .generation = 999});
}

TEST_F(WorldTest, AssignComponent) {
  World world;
  Entity e = world.create();
  auto &h = world.assign<Health>(e, 42);
  EXPECT_EQ(h.hp, 42);
}

TEST_F(WorldTest, AssignComponentDefault) {
  World world;
  Entity e = world.create();
  auto &v = world.assign<Velocity>(e);
  EXPECT_FLOAT_EQ(v.x, 0.0F);
  EXPECT_FLOAT_EQ(v.y, 0.0F);
}

TEST_F(WorldTest, AssignComponentIdempotent) {
  World world;
  Entity e = world.create();
  auto &h1 = world.assign<Health>(e, 100);
  EXPECT_EQ(h1.hp, 100);

  // second assign is a no-op (returns existing component)
  auto &h2 = world.assign<Health>(e, 200);
  EXPECT_EQ(h2.hp, 100);
  EXPECT_EQ(&h1, &h2);
}

TEST_F(WorldTest, RemoveComponent) {
  World world;
  Entity e = world.create();
  world.assign<Health>(e, 50);
  world.remove<Health>(e);

  int count = 0;
  world.view<Health>().each([&](Entity, Health &) { count++; });
  EXPECT_EQ(count, 0);
}

TEST_F(WorldTest, AssignAfterRemove) {
  World world;
  Entity e = world.create();
  world.assign<Health>(e, 10);
  world.remove<Health>(e);
  auto &h = world.assign<Health>(e, 20);
  EXPECT_EQ(h.hp, 20);
}

TEST_F(WorldTest, AssignMultipleComponents) {
  World world;
  Entity e = world.create();
  auto &h = world.assign<Health>(e, 10);
  auto &v = world.assign<Velocity>(e, 1.0F, 2.0F);
  EXPECT_EQ(h.hp, 10);
  EXPECT_FLOAT_EQ(v.x, 1.0F);
  EXPECT_FLOAT_EQ(v.y, 2.0F);
}

TEST_F(WorldTest, RemoveOneComponentLeavesOthers) {
  World world;
  Entity e = world.create();
  auto &h = world.assign<Health>(e, 77);
  world.assign<Velocity>(e, 3.0F, 4.0F);
  world.remove<Velocity>(e);

  EXPECT_EQ(h.hp, 77);
}

TEST_F(WorldTest, ViewSingleComponent) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  world.assign<Health>(a, 1);
  world.assign<Health>(b, 2);

  int count = 0;
  int sum = 0;
  world.view<Health>().each([&](Entity, Health &h) {
    count++;
    sum += h.hp;
  });
  EXPECT_EQ(count, 2);
  EXPECT_EQ(sum, 3);
}

TEST_F(WorldTest, ViewMultipleComponents) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  world.assign<Health>(a, 10);
  world.assign<Velocity>(a, 1.0F, 0.0F);
  world.assign<Health>(b, 20);

  int count = 0;
  world.view<Health, Velocity>().each(
      [&](Entity, Health &, Velocity &) { count++; });
  EXPECT_EQ(count, 1);
}

TEST_F(WorldTest, ViewSize) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  world.assign<Health>(a);
  world.assign<Health>(b);

  EXPECT_EQ(world.view<Health>().size(), 2);
  EXPECT_EQ(world.view<Velocity>().size(), 0);
}

TEST_F(WorldTest, OperationsOnStaleEntityAreSafe) {
  World world;
  Entity e = world.create();
  world.assign<Health>(e, 99);
  world.destroy(e);

  // destroy and remove silently ignore stale handles
  world.destroy(e);
  world.remove<Health>(e);

  // assign throws on stale handle
  EXPECT_THROW(world.assign<Health>(e, 1), std::runtime_error);
}

TEST_F(WorldTest, NameComponent) {
  World world;
  Entity e = world.create();
  auto &n = world.assign<Name>(e, std::string("player"));
  EXPECT_EQ(n.value, "player");
}

TEST_F(WorldTest, MeshRendererComponent) {
  World world;
  Entity e = world.create();
  MeshRenderer mr;
  mr.meshPath = "game://meshes/cube.glb";
  mr.texturePath = "game://textures/checkerboard.ktx2";
  mr.shader = "builtin://pbr";
  mr.metallic = 0.5F;
  mr.roughness = 0.3F;
  auto &got = world.assign<MeshRenderer>(e, mr);
  EXPECT_EQ(got.meshPath, "game://meshes/cube.glb");
  EXPECT_FLOAT_EQ(got.metallic, 0.5F);
  EXPECT_FLOAT_EQ(got.roughness, 0.3F);
}

TEST_F(WorldTest, EntityRecycleStress) {
  World w;
  std::vector<Entity> entities;
  entities.reserve(128);
  for (int i = 0; i < 128; i++) {
    entities.push_back(w.create());
  }

  for (auto &e : entities) {
    w.destroy(e);
  }

  // free list is LIFO: last destroyed is first recycled
  for (int i = 0; i < 128; i++) {
    Entity recycled = w.create();
    ASSERT_EQ(recycled.index, static_cast<uint32_t>(127 - i))
        << "at iteration " << i;
  }
}

TEST_F(WorldTest, ForEachComponent) {
  World world;
  Entity e = world.create();
  world.assign<Health>(e, 5);
  world.assign<Name>(e, std::string("test"));

  int visited = 0;
  world.forEachComponent(
      e, [&](Entity, ComponentId, std::string_view, void *) { visited++; });

  EXPECT_EQ(visited, 2);
}

TEST_F(WorldTest, AssignComponentWithNonTrivialMove) {
  World world;
  Entity e = world.create();
  world.assign<Name>(e, std::string("hello"));
  world.assign<Health>(e, 1);

  // fetch fresh references after archetype migration
  auto &n = world.get<Name>(e);
  auto &h = world.get<Health>(e);
  EXPECT_EQ(n.value, "hello");
  EXPECT_EQ(h.hp, 1);
}

TEST_F(WorldTest, CameraComponent) {
  World world;
  Entity e = world.create();
  Camera cam;
  cam.fov = 60.0F;
  cam.zNear = 0.01F;
  cam.zFar = 500.0F;
  cam.active = false;
  auto &c = world.assign<Camera>(e, cam);
  EXPECT_FLOAT_EQ(c.fov, 60.0F);
  EXPECT_FLOAT_EQ(c.zNear, 0.01F);
  EXPECT_FLOAT_EQ(c.zFar, 500.0F);
  EXPECT_FALSE(c.active);
}

TEST_F(WorldTest, MoveWorld) {
  World w1;
  Entity e = w1.create();
  w1.assign<Health>(e, 42);

  World w2 = std::move(w1);

  auto &h = w2.get<Health>(e);
  EXPECT_EQ(h.hp, 42);
}
