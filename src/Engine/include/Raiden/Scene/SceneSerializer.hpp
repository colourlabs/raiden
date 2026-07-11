#pragma once

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/ECS/Entity.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

namespace Raiden::ECS {
class World;
}

namespace Raiden::Scene {

struct EntityData {
  std::string name;

  bool hasTransform = false;
  glm::vec3 translation{0.F};
  glm::quat rotation{1.F, 0.F, 0.F, 0.F};
  glm::vec3 scale{1.F};
  int parentIndex = -1;

  bool hasCamera = false;
  bool cameraActive = true;
  float fov = 45.0F;
  float zNear = 0.1F;
  float zFar = 100.0F;

  bool hasMeshRenderer = false;
  std::string meshPath;
  std::string texturePath;
  std::string shader = "builtin://pbr";
  float metallic = 0.0F;
  float roughness = 0.8F;
};

struct SerializedScene {
  static constexpr int kCurrentVersion = 1;
  int version = kCurrentVersion;
  std::vector<EntityData> entities;
};

bool saveJson(const SerializedScene &scene, Core::IVirtualFileSystem &vfs,
              std::string_view path);

bool loadJson(SerializedScene &scene, Core::IVirtualFileSystem &vfs,
              std::string_view path);

bool saveBinary(const SerializedScene &scene, Core::IVirtualFileSystem &vfs,
                std::string_view path);

bool loadBinary(SerializedScene &scene, Core::IVirtualFileSystem &vfs,
                std::string_view path);

SerializedScene serializeWorld(ECS::World &world);
void deserializeWorld(const SerializedScene &scene, ECS::World &world);

} // namespace Raiden::Scene
