#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Raiden::ECS;

class TransformTest : public testing::Test {
protected:
  void SetUp() override {
    Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);
  }
};

TEST_F(TransformTest, RootEntityWorldMatrixEqualsLocal) {
  World world;
  Entity e = world.create();
  auto &t = world.assign<Transform>(e);
  t.translation = {10.0F, 20.0F, 30.0F};
  t.rotation = glm::quat({0.0F, glm::radians(90.0F), 0.0F});

  updateTransforms(world);

  glm::mat4 expected = t.localMatrix();
  glm::mat4 actual = t.worldMatrix;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      EXPECT_FLOAT_EQ(expected[i][j], actual[i][j])
          << "at [" << i << "][" << j << "]";
    }
  }
}

TEST_F(TransformTest, ChildInheritsParentTransform) {
  World world;
  Entity parent = world.create();
  Entity child = world.create();

  auto &pt = world.assign<Transform>(parent);
  pt.translation = {5.0F, 0.0F, 0.0F};

  auto &ct = world.assign<Transform>(child);
  ct.translation = {1.0F, 0.0F, 0.0F};
  ct.parent = parent;

  updateTransforms(world);

  EXPECT_FLOAT_EQ(ct.worldMatrix[3][0], 6.0F);
  EXPECT_FLOAT_EQ(ct.worldMatrix[3][1], 0.0F);
  EXPECT_FLOAT_EQ(ct.worldMatrix[3][2], 0.0F);
}

TEST_F(TransformTest, GrandchildChain) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  Entity c = world.create();

  world.assign<Transform>(a);
  auto &tb = world.assign<Transform>(b);
  tb.parent = a;
  auto &tc = world.assign<Transform>(c);
  tc.parent = b;

  tb.translation = {2.0F, 0.0F, 0.0F};
  tc.translation = {3.0F, 0.0F, 0.0F};

  updateTransforms(world);

  EXPECT_FLOAT_EQ(tc.worldMatrix[3][0], 5.0F);
}

TEST_F(TransformTest, ScaleInheritance) {
  World world;
  Entity parent = world.create();
  Entity child = world.create();

  auto &pt = world.assign<Transform>(parent);
  pt.scale = {2.0F, 2.0F, 2.0F};

  auto &ct = world.assign<Transform>(child);
  ct.translation = {1.0F, 0.0F, 0.0F};
  ct.parent = parent;

  updateTransforms(world);

  EXPECT_FLOAT_EQ(ct.worldMatrix[3][0], 2.0F);
}

TEST_F(TransformTest, Reparenting) {
  World world;
  Entity a = world.create();
  Entity b = world.create();
  Entity child = world.create();

  world.assign<Transform>(a);
  world.assign<Transform>(b);
  auto &ct = world.assign<Transform>(child);
  ct.parent = a;
  ct.translation = {10.0F, 0.0F, 0.0F};

  updateTransforms(world);
  EXPECT_FLOAT_EQ(ct.worldMatrix[3][0], 10.0F);

  ct.parent = b;
  updateTransforms(world);
  EXPECT_FLOAT_EQ(ct.worldMatrix[3][0], 10.0F);
}

TEST_F(TransformTest, MultipleChildren) {
  World world;
  Entity parent = world.create();
  world.assign<Transform>(parent);

  std::vector<Entity> children;
  for (int i = 0; i < 10; i++) {
    Entity c = world.create();
    children.push_back(c);
    auto &t = world.assign<Transform>(c);
    t.parent = parent;
    t.translation = {static_cast<float>(i), 0.0F, 0.0F};
  }

  updateTransforms(world);

  for (int i = 0; i < 10; i++) {
    auto &t = world.get<Transform>(children[static_cast<size_t>(i)]);
    EXPECT_FLOAT_EQ(t.worldMatrix[3][0], static_cast<float>(i))
        << "child " << i;
  }
}

TEST_F(TransformTest, LocalMatrixRotation) {
  World world;
  Entity e = world.create();
  auto &t = world.assign<Transform>(e);
  t.rotation =
      glm::angleAxis(glm::radians(180.0F), glm::vec3{0.0F, 1.0F, 0.0F});
  t.translation = {1.0F, 0.0F, 0.0F};

  glm::mat4 local = t.localMatrix();
  glm::mat4 expected = glm::translate(glm::mat4{1.0F}, {1.0F, 0.0F, 0.0F}) *
                       glm::mat4_cast(t.rotation);

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      EXPECT_FLOAT_EQ(expected[i][j], local[i][j])
          << "at [" << i << "][" << j << "]";
    }
  }
}
