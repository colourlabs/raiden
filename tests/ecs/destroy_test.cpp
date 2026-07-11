#include <gtest/gtest.h>
#include <Raiden/ECS/World.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/Logger.hpp>

using namespace Raiden::ECS;

TEST(DestroyTest, DestroySingleWithComponents) {
  Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);

  World w;
  Entity e = w.create();
  w.assign<Transform>(e);
  w.assign<Name>(e, std::string("test"));
  w.destroy(e);
}

TEST(DestroyTest, DestroyTwoInterleaved) {
  Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);

  World w;
  Entity a = w.create();
  Entity b = w.create();
  w.assign<Transform>(a);
  w.assign<Name>(a, std::string("a"));
  w.assign<Transform>(b);
  w.assign<Name>(b, std::string("b"));
  w.destroy(b);
  w.destroy(a);
}

TEST(DestroyTest, DestroyTwoSequential) {
  Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);

  World w;
  Entity a = w.create();
  w.assign<Transform>(a);
  w.assign<Name>(a, std::string("a"));
  Entity b = w.create();
  w.assign<Transform>(b);
  w.assign<Name>(b, std::string("b"));
  w.destroy(b);
  w.destroy(a);
}

TEST(DestroyTest, DestroyAllAndWorldDestructor) {
  Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);

  World w;
  std::vector<Entity> entities;

  for (int i = 0; i < 65; i++) {
    Entity e = w.create();
    w.assign<Transform>(e);
    w.assign<Name>(e, std::string("test"));
    entities.push_back(e);
  }

  for (auto e : entities) {
    w.destroy(e);
  }
}

TEST(DestroyTest, DestroyManyStress) {
  Raiden::Core::Logger::setMinLogLevel(Raiden::Core::LogLevel::Critical);
  for (int iter = 0; iter < 10; iter++) {
    World w;
    std::vector<Entity> entities;
    for (int i = 0; i < 100; i++) {
      Entity e = w.create();
      w.assign<Transform>(e);
      w.assign<Name>(e, std::string("test"));
      entities.push_back(e);
    }
    for (auto e : entities) {
      w.destroy(e);
    }
  }
}
