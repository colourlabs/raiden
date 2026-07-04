#include <Raiden/ECS/World.hpp>

namespace Raiden::ECS {

World::World() { rootArchetype_ = &archetypes_[0]; }

World::~World() {
  // destruct all components in all archetypes
  for (auto &[mask, arch] : archetypes_) {
    for (size_t i = 0; i < arch.signature.size(); ++i) {
      auto cid = arch.signature[i];
      auto &info = componentInfo_[cid];
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
  slot.archetype = rootArchetype_;
  slot.row =
      rootArchetype_->add(Entity{.index = idx, .generation = slot.generation});
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
    auto &info = componentInfo_[cid];
    if (info.destruct != nullptr) {
        info.destruct(arch->data(static_cast<int32_t>(i), slot.row));
    }
  }

  // remove from archetype (destructs last-row components via callback)
  int32_t oldRow = slot.row;
  Entity moved = arch->swapRemove(oldRow, [&](int32_t r) {
    for (size_t i = 0; i < arch->signature.size(); ++i) {
      auto cid = arch->signature[i];
      auto &cinfo = componentInfo_[cid];
      if (cinfo.destruct) {
        cinfo.destruct(arch->data(static_cast<int32_t>(i), r));
      }
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

  auto it = archetypes_.find(mask);
  if (it != archetypes_.end()) {
    return &it->second;
  }

  // build component sizes for the new archetype
  std::vector<size_t> sizes;
  sizes.reserve(sig.size());
  for (auto cid : sig) {
    sizes.push_back(componentInfo_[cid].size);
  }

  auto &arch = archetypes_[mask];
  arch.mask = mask;
  arch.signature = sig;
  arch.componentSizes = std::move(sizes);
  arch.columns_.resize(arch.signature.size());
  return &arch;
}

} // namespace Raiden::ECS
