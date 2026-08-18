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
  std::string normalMap;
  std::string metallicRoughnessMap;
  std::string occlusionMap;
  std::string shader = "builtin://pbr";
  glm::vec4 baseColorFactor{1.0F, 1.0F, 1.0F, 1.0F};
  float metallic = 0.0F;
  float roughness = 0.8F;
  glm::vec2 uvScale{1.0F, 1.0F};
  bool triplanarMapping = false;

  bool hasDirectionalLight = false;
  glm::vec3 dlDirection{0.3F, -1.0F, 0.5F};
  glm::vec3 dlColor{1.0F, 0.98F, 0.92F};
  float dlIntensity = 1.0F;
  bool dlCastShadows = true;
  float dlShadowNear = 0.1F;
  float dlShadowFar = 50.0F;
  float dlShadowSize = 20.0F;
  uint32_t dlShadowMapResolution = 2048;

  bool hasPointLight = false;
  glm::vec3 plPosition{0.0F};
  glm::vec3 plColor{1.0F, 1.0F, 1.0F};
  float plIntensity = 1.0F;
  float plRange = 10.0F;

  bool hasAmbientLight = false;
  glm::vec3 alSkyColor{0.1F, 0.15F, 0.3F};
  float alSkyIntensity = 1.0F;
  glm::vec3 alGroundColor{0.02F, 0.01F, 0.01F};
  bool alUseIBL = false;

  bool hasRigidbody = false;
  uint8_t rigidbodyType = 0;
  float rigidbodyMass = 1.0F;
  float rigidbodyFriction = 0.5F;
  float rigidbodyRestitution = 0.1F;

  bool hasCollider = false;
  uint8_t colliderShape = 0;
  glm::vec3 colliderHalfExtents{0.5F};
  float colliderRadius = 0.5F;
  float colliderHeight = 1.0F;
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
