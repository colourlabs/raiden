#pragma once

#include "Archetype.hpp"
#include "Entity.hpp"

#include <cstring>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Raiden::Core {

static constexpr uint32_t kMaxComponentTypes = 64;

// entity slot (internal)

struct Slot {
  uint32_t generation = 0;
  Archetype *archetype = nullptr;
  int32_t row = -1;
  uint32_t nextFree = uint32_t(-1);
};

// component type metadata

struct ComponentInfo {
  size_t size;
  void (*destruct)(void *);
  void (*move)(void *dst, void *src);
  std::string_view name;
  uint32_t bitIndex;
};

template <typename T> void componentDestruct(void *p) {
  static_cast<T *>(p)->~T();
}

template <typename T> void componentMove(void *dst, void *src) {
  if constexpr (std::is_trivially_copyable_v<T>) {
    std::memcpy(dst, src, sizeof(T));
  } else {
    T *s = static_cast<T *>(src);
    T *d = static_cast<T *>(dst);
    if constexpr (std::is_move_constructible_v<T>)
      *d = std::move(*s);
    else
      *d = *s;
  }
}

// view (iterable query result)

template <typename... Ts> struct View {
  std::vector<Archetype *> archetypes;

  template <typename F> void each(F &&fn) {
    for (auto *arch : archetypes) {
      auto cols = std::array<int32_t, sizeof...(Ts)>{
          arch->indexOf(componentId<Ts>())...};
      size_t count = arch->entities.size();
      for (size_t r = 0; r < count; ++r) {
        [&]<size_t... I>(std::index_sequence<I...>) {
          fn(arch->entities[r],
             *(Ts *)(arch->data(cols[I], static_cast<int32_t>(r)))...);
        }(std::index_sequence_for<Ts...>{});
      }
    }
  }

  size_t size() const {
    size_t total = 0;
    for (auto *a : archetypes)
      total += a->entities.size();
    return total;
  }
};

// world (registry)

class World {
public:
  World();
  ~World();

  World(const World &) = delete;
  World &operator=(const World &) = delete;

  Entity create();
  void destroy(Entity e);

  template <typename T, typename... Args>
  void assign(Entity e, Args &&...args) {
    registerIfNew<T>();
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation)
      return;

    Archetype *src = slot.archetype;

    // build destination signature
    std::vector<ComponentId> sig = src->signature;
    auto it = std::lower_bound(sig.begin(), sig.end(), componentId<T>());
    if (it != sig.end() && *it == componentId<T>())
      return; // already has it
    sig.insert(it, componentId<T>());

    Archetype *dst = findOrCreateArchetype(sig);
    int32_t dstRow = dst->add(Entity{e.index, slot.generation});

    // move existing components from src to dst using proper move semantics
    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      auto &cinfo = componentInfo_[cid];
      auto col = dst->indexOf(cid);
      cinfo.move(dst->data(col, dstRow), src->data(static_cast<int32_t>(i), slot.row));
    }

    // destruct source components (moved-from)
    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      auto &cinfo = componentInfo_[cid];
      if (cinfo.destruct)
        cinfo.destruct(src->data(static_cast<int32_t>(i), slot.row));
    }

    // construct the new component
    auto &info = componentInfo_[componentId<T>()];
    new (dst->data(dst->indexOf(componentId<T>()), dstRow))
        T(std::forward<Args>(args)...);

    // remove from src (destructs last-row components via callback)
    int32_t oldRow = slot.row;
    auto *srcArch = src;
    Entity moved = srcArch->swapRemove(oldRow, [&](int32_t r) {
      for (size_t i = 0; i < srcArch->signature.size(); ++i) {
        auto cid = srcArch->signature[i];
        auto &cinfo = componentInfo_[cid];
        if (cinfo.destruct)
          cinfo.destruct(srcArch->data(static_cast<int32_t>(i), r));
      }
    });
    if (moved.index != uint32_t(-1))
      slots_[moved.index].row = oldRow;

    slot.archetype = dst;
    slot.row = dstRow;
  }

  template <typename T> void remove(Entity e) {
    registerIfNew<T>();
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation)
      return;

    Archetype *src = slot.archetype;

    std::vector<ComponentId> sig;
    for (auto id : src->signature)
      if (id != componentId<T>())
        sig.push_back(id);

    if (sig.size() == src->signature.size())
      return; // didn't have it

    Archetype *dst = findOrCreateArchetype(sig);
    int32_t dstRow = dst->add(Entity{e.index, slot.generation});

    // move all except the removed component
    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      if (cid == componentId<T>())
        continue;
      auto &cinfo = componentInfo_[cid];
      auto col = dst->indexOf(cid);
      cinfo.move(dst->data(col, dstRow), src->data(static_cast<int32_t>(i), slot.row));
    }

    // destruct removed component
    auto &rinfo = componentInfo_[componentId<T>()];
    auto remCol = src->indexOf(componentId<T>());
    if (rinfo.destruct)
      rinfo.destruct(src->data(remCol, slot.row));

    // remove from src (destructs last-row components via callback)
    int32_t oldRow = slot.row;
    auto *srcArch = src;
    Entity moved = srcArch->swapRemove(oldRow, [&](int32_t r) {
      for (size_t i = 0; i < srcArch->signature.size(); ++i) {
        auto cid = srcArch->signature[i];
        auto &cinfo = componentInfo_[cid];
        if (cinfo.destruct)
          cinfo.destruct(srcArch->data(static_cast<int32_t>(i), r));
      }
    });
    if (moved.index != uint32_t(-1))
      slots_[moved.index].row = oldRow;

    slot.archetype = dst;
    slot.row = dstRow;
  }

  template <typename T> T &get(Entity e) {
    auto &slot = slots_[e.index];
    (void)slot.generation;
    auto &info = registerIfNew<T>();
    auto col = slot.archetype->indexOf(componentId<T>());
    return *(T *)(slot.archetype->data(col, slot.row));
  }

  template <typename... Ts> auto view() {
    (registerIfNew<Ts>(), ...);

    uint64_t queryMask = 0;
    ((queryMask |=
       (uint64_t(1) << componentInfo_[componentId<Ts>()].bitIndex)),
     ...);

    View<Ts...> v;
    for (auto &[mask, arch] : archetypes_) {
      if ((mask & queryMask) == queryMask)
        v.archetypes.push_back(&arch);
    }
    return v;
  }

  // set a human-readable name for a component type (used by forEachComponent)
  template <typename T> void registerComponent(std::string_view name) {
    registerIfNew<T>().name = name;
  }

  // iterate all components on an entity without knowing types at compile time
  // visitor receives (Entity, ComponentId, std::string_view name, void *data)
  template <typename F> void forEachComponent(Entity e, F &&visitor) {
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation)
      return;
    auto *arch = slot.archetype;
    for (size_t i = 0; i < arch->signature.size(); ++i) {
      auto cid = arch->signature[i];
      auto &info = componentInfo_[cid];
      void *comp = arch->data(static_cast<int32_t>(i), slot.row);
      visitor(e, cid, info.name, comp);
    }
  }

private:
  ComponentInfo &ensureComponent(ComponentId id, size_t sz,
                                  void (*dtor)(void *) = nullptr,
                                  void (*mv)(void *, void *) = nullptr) {
    auto it = componentInfo_.find(id);
    if (it != componentInfo_.end())
      return it->second;
    uint32_t bit = nextComponentBit_++;
    return componentInfo_
        .emplace(id, ComponentInfo{sz, dtor, mv, {}, bit})
        .first->second;
  }

  // register Ts... in view() if not already known
  template <typename T> ComponentInfo &registerIfNew() {
    return ensureComponent(componentId<T>(), sizeof(T), componentDestruct<T>,
                           componentMove<T>);
  }
  Archetype *findOrCreateArchetype(const std::vector<ComponentId> &sig);

  std::vector<Slot> slots_;
  uint32_t freeHead_ = uint32_t(-1);
  std::unordered_map<uint64_t, Archetype> archetypes_;
  Archetype *rootArchetype_ = nullptr;
  uint32_t nextComponentBit_ = 0;
  std::unordered_map<ComponentId, ComponentInfo> componentInfo_;
};

} // namespace Raiden::Core
