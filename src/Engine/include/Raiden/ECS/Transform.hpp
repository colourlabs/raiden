#pragma once

#include "World.hpp"
#include <Raiden/Jobs/JobSystem.hpp>

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace Raiden::ECS {

struct Transform {
  glm::vec3 translation{0.0F};
  glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
  glm::vec3 scale{1.0F};
  glm::mat4 worldMatrix{1.0F};
  Entity parent = NullEntity;

  glm::mat4 localMatrix() const {
    return glm::translate(glm::mat4{1.0F}, translation) *
           glm::mat4_cast(rotation) * glm::scale(glm::mat4{1.0F}, scale);
  }
};

inline void updateTransforms(World &world) {
  std::vector<uint8_t> computed(world.entityCount(), 0);

  auto compute = [&](Entity e, auto &self) -> void {
    if (computed[e.index]) {
      return;
    }
    auto &t = world.get<Transform>(e);
    if (t.parent == NullEntity) {
      t.worldMatrix = t.localMatrix();
    } else {
      self(t.parent, self);
      auto &pt = world.get<Transform>(t.parent);
      t.worldMatrix = pt.worldMatrix * t.localMatrix();
    }
    computed[e.index] = 1;
  };

  world.view<Transform>().each(
      [&](Entity e, Transform &) { compute(e, compute); });
}

inline void updateTransforms(World &world, ::Raiden::Jobs::JobSystem &js) {
  struct Entry {
    Entity e;
    Transform *t;
    int depth;
  };
  std::vector<Entry> entries;

  world.view<Transform>().each(
      [&](Entity e, Transform &t) { entries.push_back({e, &t, 0}); });

  if (entries.empty()) {
    return;
  }

  // compute depth using flat cache (no hash map)
  std::vector<uint8_t> depthKnown(world.entityCount(), 0);
  std::vector<int> depthValues(world.entityCount(), 0);

  auto getDepth = [&](Entity e, auto &self) -> int {
    if (depthKnown[e.index]) {
      return depthValues[e.index];
    }
    auto &t = world.get<Transform>(e);
    if (t.parent == NullEntity) {
      depthKnown[e.index] = 1;
      depthValues[e.index] = 0;
      return 0;
    }
    int d = 1 + self(t.parent, self);
    depthKnown[e.index] = 1;
    depthValues[e.index] = d;
    return d;
  };

  for (auto &entry : entries) {
    entry.depth = getDepth(entry.e, getDepth);
  }

  // sort by depth so parents precede children
  std::ranges::sort(entries,
                    [](auto &a, auto &b) { return a.depth < b.depth; });

  int maxDepth = entries.back().depth;
  std::vector<std::vector<Entry *>> byDepth(static_cast<size_t>(maxDepth) + 1);
  for (auto &entry : entries) {
    byDepth[static_cast<size_t>(entry.depth)].push_back(&entry);
  }

  for (auto &level : byDepth) {
    if (level.size() >= 16) {
      js.parallelFor(0, static_cast<uint32_t>(level.size()), 1,
                     [&](uint32_t begin, uint32_t end) {
                       for (uint32_t i = begin; i < end; ++i) {
                         auto &t = *level[i]->t;
                         if (t.parent == NullEntity) {
                           t.worldMatrix = t.localMatrix();
                         } else {
                           auto &pt = world.get<Transform>(t.parent);
                           t.worldMatrix = pt.worldMatrix * t.localMatrix();
                         }
                       }
                     });
    } else {
      for (auto *ep : level) {
        auto &t = *ep->t;
        if (t.parent == NullEntity) {
          t.worldMatrix = t.localMatrix();
        } else {
          auto &pt = world.get<Transform>(t.parent);
          t.worldMatrix = pt.worldMatrix * t.localMatrix();
        }
      }
    }
  }
}

} // namespace Raiden::ECS
