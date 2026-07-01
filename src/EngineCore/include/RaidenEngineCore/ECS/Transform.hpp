#pragma once

#include "World.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Raiden::Core {

struct Transform {
  glm::vec3 translation{0.f};
  glm::quat rotation{1.f, 0.f, 0.f, 0.f};
  glm::vec3 scale{1.f};
  glm::mat4 worldMatrix{1.f};
  Entity parent = NullEntity;

  glm::mat4 localMatrix() const {
    return glm::translate(glm::mat4{1.f}, translation) *
           glm::mat4_cast(rotation) * glm::scale(glm::mat4{1.f}, scale);
  }
};

inline void updateTransforms(World &world) {
  world.view<Transform>().each([&](Entity, Transform &t) {
    if (t.parent == NullEntity) {
      t.worldMatrix = t.localMatrix();
    } else {
      auto &pt = world.get<Transform>(t.parent);
      t.worldMatrix = pt.worldMatrix * t.localMatrix();
    }
  });
}

} // namespace Raiden::Core
