#include <RaidenEngineCore/ECS/World.hpp>

namespace Raiden::Core {

World::World() { rootArchetype_ = &archetypes_[{}]; }

World::~World() {
  // destruct all components in all archetypes
  for (auto &[sig, arch] : archetypes_) {
    for (size_t i = 0; i < sig.size(); ++i) {
      auto cid = sig[i];
      auto &info = componentInfo_[cid];
      if (!info.destruct)
        continue;
      for (int32_t r = 0; r < static_cast<int32_t>(arch.entities.size()); ++r)
        info.destruct(arch.column(static_cast<int32_t>(i)) + r * info.size);
    }
  }
}

Entity World::create() {
  uint32_t idx;
  if (freeHead_ != uint32_t(-1)) {
    idx = freeHead_;
    freeHead_ = slots_[idx].nextFree;
  } else {
    idx = static_cast<uint32_t>(slots_.size());
    slots_.emplace_back();
  }

  auto &slot = slots_[idx];
  slot.generation++;
  slot.archetype = rootArchetype_;
  slot.row = rootArchetype_->add(Entity{idx, slot.generation});
  return {idx, slot.generation};
}

void World::destroy(Entity e) {
  auto &slot = slots_[e.index];
  if (slot.generation != e.generation)
    return;

  auto *arch = slot.archetype;
  for (size_t i = 0; i < arch->signature.size(); ++i) {
    auto cid = arch->signature[i];
    auto &info = componentInfo_[cid];
    if (info.destruct)
      info.destruct(arch->column(i) + slot.row * info.size);
  }

  Entity moved = arch->swapRemove(slot.row);
  if (moved.index != uint32_t(-1))
    slots_[moved.index].row = slot.row;

  slot.archetype = nullptr;
  slot.row = -1;
  slot.generation++;
  slot.nextFree = freeHead_;
  freeHead_ = e.index;
}

Archetype *World::findOrCreateArchetype(const std::vector<ComponentId> &sig) {
  auto it = archetypes_.find(sig);
  if (it != archetypes_.end())
    return &it->second;

  // build component sizes for the new archetype
  std::vector<size_t> sizes;
  sizes.reserve(sig.size());
  for (auto cid : sig)
    sizes.push_back(componentInfo_[cid].size);

  auto &arch = archetypes_[sig];
  arch.signature = sig;
  arch.componentSizes = std::move(sizes);
  arch.columns.resize(arch.signature.size());
  return &arch;
}

} // namespace Raiden::Core
