#include <gtest/gtest.h>
#include <Raiden/ECS/World.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/Scene/SceneSerializer.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <glm/gtc/quaternion.hpp>

using namespace Raiden::ECS;
using namespace Raiden::Scene;

class SceneSerializerTest : public testing::Test {
protected:
  void SetUp() override {
    Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);
    vfs = Raiden::Core::createOSFileSystem();
    vfs->registerData("test://placeholder", {std::byte{0}});
  }

public:
  std::unique_ptr<Raiden::Core::IVirtualFileSystem> vfs;
};

TEST_F(SceneSerializerTest, EmptySceneRoundTrip) {
  SerializedScene scene;
  scene.version = 1;

  EXPECT_TRUE(saveJson(scene, *vfs, "test://empty.json"));
  EXPECT_TRUE(vfs->exists("test://empty.json"));

  SerializedScene loaded;
  EXPECT_TRUE(loadJson(loaded, *vfs, "test://empty.json"));
  EXPECT_EQ(loaded.entities.size(), 0);
}

TEST_F(SceneSerializerTest, SingleEntityRoundTrip) {
  SerializedScene scene;
  EntityData e;
  e.name = "test_entity";
  e.hasTransform = true;
  e.translation = {1.0F, 2.0F, 3.0F};
  e.rotation = {0.707F, 0.0F, 0.707F, 0.0F};
  e.scale = {2.0F, 2.0F, 2.0F};
  scene.entities.push_back(e);

  EXPECT_TRUE(saveJson(scene, *vfs, "test://single.json"));

  SerializedScene loaded;
  EXPECT_TRUE(loadJson(loaded, *vfs, "test://single.json"));
  ASSERT_EQ(loaded.entities.size(), 1);
  EXPECT_EQ(loaded.entities[0].name, "test_entity");
  EXPECT_TRUE(loaded.entities[0].hasTransform);
  EXPECT_FLOAT_EQ(loaded.entities[0].translation.x, 1.0F);
  EXPECT_FLOAT_EQ(loaded.entities[0].translation.y, 2.0F);
  EXPECT_FLOAT_EQ(loaded.entities[0].translation.z, 3.0F);
  EXPECT_FLOAT_EQ(loaded.entities[0].rotation.w, 0.707F);
  EXPECT_FLOAT_EQ(loaded.entities[0].scale.x, 2.0F);
}

TEST_F(SceneSerializerTest, FullComponentsRoundTrip) {
  SerializedScene scene;

  EntityData e;
  e.name = "full_entity";
  e.hasTransform = true;
  e.translation = {10.0F, 20.0F, 30.0F};
  e.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
  e.scale = {1.0F, 1.0F, 1.0F};
  e.hasCamera = true;
  e.cameraActive = true;
  e.fov = 60.0F;
  e.zNear = 0.01F;
  e.zFar = 1000.0F;
  e.hasMeshRenderer = true;
  e.meshPath = "game://meshes/cube.glb";
  e.texturePath = "game://textures/checkerboard.ktx2";
  e.shader = "builtin://pbr";
  e.metallic = 0.5F;
  e.roughness = 0.2F;
  scene.entities.push_back(e);

  // save and load JSON
  EXPECT_TRUE(saveJson(scene, *vfs, "test://full.json"));
  SerializedScene loaded;
  EXPECT_TRUE(loadJson(loaded, *vfs, "test://full.json"));

  ASSERT_EQ(loaded.entities.size(), 1);
  auto &l = loaded.entities[0];
  EXPECT_EQ(l.name, "full_entity");
  EXPECT_TRUE(l.hasTransform);
  EXPECT_TRUE(l.hasCamera);
  EXPECT_TRUE(l.hasMeshRenderer);
  EXPECT_FLOAT_EQ(l.fov, 60.0F);
  EXPECT_FLOAT_EQ(l.metallic, 0.5F);
  EXPECT_FLOAT_EQ(l.roughness, 0.2F);
  EXPECT_EQ(l.meshPath, "game://meshes/cube.glb");
}

TEST_F(SceneSerializerTest, BinaryRoundTrip) {
  SerializedScene scene;

  EntityData e;
  e.name = "binary_test";
  e.hasTransform = true;
  e.translation = {1.0F, 2.0F, 3.0F};
  e.rotation = {0.0F, 0.0F, 0.0F, 1.0F};
  e.scale = {1.0F, 1.0F, 1.0F};
  e.hasCamera = true;
  e.fov = 90.0F;
  e.zNear = 0.1F;
  e.zFar = 500.0F;
  e.hasMeshRenderer = true;
  e.meshPath = "test.glb";
  e.texturePath = "test.ktx2";
  e.shader = "builtin://pbr";
  e.metallic = 1.0F;
  e.roughness = 0.0F;
  scene.entities.push_back(e);

  EXPECT_TRUE(saveBinary(scene, *vfs, "test://scene.bin"));
  ASSERT_TRUE(vfs->exists("test://scene.bin"));

  SerializedScene loaded;
  EXPECT_TRUE(loadBinary(loaded, *vfs, "test://scene.bin"));

  ASSERT_EQ(loaded.entities.size(), 1);
  auto &l = loaded.entities[0];
  EXPECT_EQ(l.name, "binary_test");
  EXPECT_TRUE(l.hasTransform);
  EXPECT_TRUE(l.hasCamera);
  EXPECT_TRUE(l.hasMeshRenderer);
  EXPECT_FLOAT_EQ(l.fov, 90.0F);
  EXPECT_FLOAT_EQ(l.metallic, 1.0F);
  EXPECT_FLOAT_EQ(l.roughness, 0.0F);
  EXPECT_EQ(l.meshPath, "test.glb");
  EXPECT_EQ(l.texturePath, "test.ktx2");
  EXPECT_EQ(l.shader, "builtin://pbr");
}

TEST_F(SceneSerializerTest, BinaryEmptyScene) {
  SerializedScene scene;
  EXPECT_TRUE(saveBinary(scene, *vfs, "test://empty.bin"));

  SerializedScene loaded;
  EXPECT_TRUE(loadBinary(loaded, *vfs, "test://empty.bin"));
  EXPECT_EQ(loaded.entities.size(), 0);
}

TEST_F(SceneSerializerTest, BinaryMultipleEntities) {
  SerializedScene scene;
  for (int i = 0; i < 5; i++) {
    EntityData e;
    e.name = "entity_" + std::to_string(i);
    e.hasTransform = true;
    e.translation = {static_cast<float>(i), 0.0F, 0.0F};
    scene.entities.push_back(e);
  }

  EXPECT_TRUE(saveBinary(scene, *vfs, "test://multi.bin"));

  SerializedScene loaded;
  EXPECT_TRUE(loadBinary(loaded, *vfs, "test://multi.bin"));
  ASSERT_EQ(loaded.entities.size(), 5);

  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(loaded.entities[static_cast<size_t>(i)].name, "entity_" + std::to_string(i));
    EXPECT_FLOAT_EQ(loaded.entities[static_cast<size_t>(i)].translation.x, static_cast<float>(i));
  }
}

TEST_F(SceneSerializerTest, WorldRoundTrip) {
  World w;

  Entity parent = w.create();
  w.assign<Name>(parent, std::string("parent"));
  auto &pt = w.assign<Transform>(parent);
  pt.translation = {5.0F, 0.0F, 0.0F};

  Entity child = w.create();
  w.assign<Name>(child, std::string("child"));
  auto &ct = w.assign<Transform>(child);
  ct.translation = {1.0F, 0.0F, 0.0F};
  ct.parent = parent;

  Entity cam = w.create();
  auto &c = w.assign<Camera>(cam);
  c.fov = 75.0F;

  updateTransforms(w);

  SerializedScene serialized = serializeWorld(w);
  EXPECT_EQ(serialized.entities.size(), 2); // only entities with Transform

  // deserialize into a fresh world
  World w2;
  deserializeWorld(serialized, w2);

  int count = 0;
  w2.view<Transform>().each([&](Entity, Transform &t) {
    count++;
    EXPECT_FLOAT_EQ(t.worldMatrix[3][0], t.parent == NullEntity ? 5.0F : 6.0F);
  });
  EXPECT_EQ(count, 2);
}

TEST_F(SceneSerializerTest, SceneFileNotFound) {
  SerializedScene scene;
  EXPECT_FALSE(loadJson(scene, *vfs, "test://nonexistent.json"));
  EXPECT_FALSE(loadBinary(scene, *vfs, "test://nonexistent.bin"));
}
