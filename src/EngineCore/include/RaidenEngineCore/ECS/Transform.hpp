#pragma once

#include "World.hpp"
#include <RaidenEngineCore/Jobs/JobSystem.hpp>

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

inline void updateTransforms(World &world, JobSystem &js) {
  auto view = world.view<Transform>();
  if (view.archetypes.empty())
    return;

  auto &archetypes = view.archetypes;
  auto transformId = componentId<Transform>();

  js.parallelFor(0, static_cast<uint32_t>(archetypes.size()), 1,
                 [&](uint32_t begin, uint32_t end) {
                   for (uint32_t a = begin; a < end; ++a) {
                     auto *arch = archetypes[a];
                     auto col = arch->indexOf(transformId);
                     if (col == -1)
                       continue;
                     size_t count = arch->entities.size();
                     for (size_t r = 0; r < count; ++r) {
                       auto &t = *(Transform *)(arch->column(col) +
                                                r * sizeof(Transform));
                       if (t.parent == NullEntity) {
                         t.worldMatrix = t.localMatrix();
                       } else {
                         auto &pt = world.get<Transform>(t.parent);
                         t.worldMatrix = pt.worldMatrix * t.localMatrix();
                       }
                     }
                   }
                 });
}

} // namespace Raiden::Core
