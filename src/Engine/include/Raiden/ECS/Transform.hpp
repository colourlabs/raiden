#pragma once

#include "World.hpp"
#include <Raiden/Jobs/JobSystem.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
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
  // collect entities with hierarchy depth so parents are processed first
  struct Entry {
    Entity e;
    int depth;
  };
  std::vector<Entry> entries;

  world.view<Transform>().each([&](Entity e, Transform &t) {
    int depth = 0;
    Entity p = t.parent;
    while (p != NullEntity) {
      depth++;
      p = world.get<Transform>(p).parent;
    }
    entries.push_back({e, depth});
  });

  // sort by depth so parents always precede children
  std::sort(entries.begin(), entries.end(),
            [](auto &a, auto &b) { return a.depth < b.depth; });

  for (auto &[e, depth] : entries) {
    (void)depth;
    auto &t = world.get<Transform>(e);
    if (t.parent == NullEntity) {
      t.worldMatrix = t.localMatrix();
    } else {
      auto &pt = world.get<Transform>(t.parent);
      t.worldMatrix = pt.worldMatrix * t.localMatrix();
    }
  }
}

inline void updateTransforms(World &world, ::Raiden::Jobs::JobSystem &js) {
  struct Entry {
    Entity e;
    int depth;
  };
  std::vector<Entry> entries;

  world.view<Transform>().each([&](Entity e, Transform &t) {
    int depth = 0;
    Entity p = t.parent;
    while (p != NullEntity) {
      depth++;
      p = world.get<Transform>(p).parent;
    }
    entries.push_back({e, depth});
  });

  if (entries.empty())
    return;

  // sort by depth
  std::sort(entries.begin(), entries.end(),
            [](auto &a, auto &b) { return a.depth < b.depth; });

  // group by depth level; entities at the same depth have no dependencies
  // on each other and can be processed in parallel
  int maxDepth = entries.back().depth;
  std::vector<std::vector<Entry>> byDepth(static_cast<size_t>(maxDepth) + 1);
  for (auto &entry : entries)
    byDepth[static_cast<size_t>(entry.depth)].push_back(entry);

  for (auto &level : byDepth) {
    if (level.size() >= 16) {
      js.parallelFor(0, static_cast<uint32_t>(level.size()), 1,
                     [&](uint32_t begin, uint32_t end) {
                       for (uint32_t i = begin; i < end; ++i) {
                         auto &[e, _] = level[i];
                         auto &t = world.get<Transform>(e);
                         if (t.parent == NullEntity) {
                           t.worldMatrix = t.localMatrix();
                         } else {
                           auto &pt = world.get<Transform>(t.parent);
                           t.worldMatrix = pt.worldMatrix * t.localMatrix();
                         }
                       }
                     });
    } else {
      for (auto &[e, _] : level) {
        auto &t = world.get<Transform>(e);
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
