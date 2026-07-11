#include <Raiden/ECS/World.hpp>

namespace Raiden::ECS {

World::World() {
  // ensure root archetype exists and is stable
  rootArchetype();
}

Archetype &World::rootArchetype() {
  auto *ptr = lookupArchetype(0);
  if (ptr != nullptr) {
    return *ptr;
  }
  // signature is empty for root
  return createArchetype(0, {}, {});
}

Archetype *World::lookupArchetype(uint64_t mask) {
  auto it = archetypeIndex_.find(mask);
  if (it != archetypeIndex_.end()) {
    return it->second;
  }
  return nullptr;
}

Archetype &World::createArchetype(uint64_t mask, std::vector<ComponentId> sig,
                                  std::vector<size_t> sizes) {
  archetypes_.emplace_back(mask, std::move(sig), std::move(sizes));
  auto *ptr = &archetypes_.back();
  archetypeIndex_[mask] = ptr;
  return *ptr;
}

World::~World() {
  for (auto &arch : archetypes_) {
    for (size_t i = 0; i < arch.signature.size(); ++i) {
      auto cid = arch.signature[i];
      auto it = componentInfo_.find(cid);
      if (it == componentInfo_.end()) {
        continue;
      }
      auto &info = it->second;
      if (info.destruct == nullptr) {
        continue;
      }
      for (size_t r = 0; r < arch.entities.size(); ++r) {
        info.destruct(arch.data(i, static_cast<int32_t>(r)));
      }
    }
  }
}

Entity World::create() {
  uint32_t idx = 0;
  if (freeHead_ != UINT32_MAX) {
    idx = freeHead_;
    freeHead_ = slots_[idx].nextFree;
  } else {
    idx = static_cast<uint32_t>(slots_.size());
    slots_.emplace_back();
  }

  auto &slot = slots_[idx];
  slot.generation++;
  auto &root = rootArchetype();
  slot.archetype = &root;
  slot.row = root.add(Entity{.index = idx, .generation = slot.generation});
  return {.index = idx, .generation = slot.generation};
}

void World::destroy(Entity e) {
  auto &slot = slots_[e.index];
  if (slot.generation != e.generation) {
    return;
  }

  auto *arch = slot.archetype;

  // destruct all components
  for (size_t i = 0; i < arch->signature.size(); ++i) {
    auto cid = arch->signature[i];
    auto it = componentInfo_.find(cid);
    if (it == componentInfo_.end()) {
      continue;
    }
    auto &info = it->second;
    if (info.destruct != nullptr) {
        info.destruct(arch->data(static_cast<int32_t>(i), slot.row));
    }
  }

  // remove from archetype (moves last-row components then destructs them)
  int32_t oldRow = slot.row;
  Entity moved = arch->swapRemove(
      oldRow,
      [&](int32_t col, int32_t dstRow, int32_t srcRow) {
        auto cid = arch->signature[col];
        auto it = componentInfo_.find(cid);
        if (it == componentInfo_.end()) {
          return;
        }
        it->second.move(arch->data(col, dstRow), arch->data(col, srcRow));
      },
      [&](int32_t col, int32_t row) {
        auto cid = arch->signature[col];
        auto it = componentInfo_.find(cid);
        if (it == componentInfo_.end()) {
          return;
        }
        auto &cinfo = it->second;
        if (cinfo.destruct) {
          cinfo.destruct(arch->data(col, row));
        }
      });
  if (moved.index != UINT32_MAX) {
    slots_[moved.index].row = oldRow;
  }

  slot.archetype = nullptr;
  slot.row = -1;
  slot.generation++;
  slot.nextFree = freeHead_;
  freeHead_ = e.index;
}

Archetype *World::findOrCreateArchetype(const std::vector<ComponentId> &sig) {
  // build mask from signature
  uint64_t mask = 0;
  for (auto cid : sig) {
    auto it = componentInfo_.find(cid);
    if (it != componentInfo_.end()) {
      mask |= (uint64_t(1) << it->second.bitIndex);
    }
  }

  auto *existing = lookupArchetype(mask);
  if (existing != nullptr) {
    return existing;
  }

  // build component sizes for the new archetype
  std::vector<size_t> sizes;
  sizes.reserve(sig.size());
  for (auto cid : sig) {
    sizes.push_back(componentInfo_[cid].size);
  }

  return &createArchetype(mask, sig, std::move(sizes));
}

} // namespace Raiden::ECS
