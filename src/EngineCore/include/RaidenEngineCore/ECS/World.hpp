#pragma once

#include "Archetype.hpp"
#include "Entity.hpp"

#include <cstring>
#include <map>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Raiden::Core {

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
             *(Ts *)(arch->column(cols[I]) + r * sizeof(Ts))...);
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

    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      size_t sz = componentInfo_[cid].size;
      auto col = dst->indexOf(cid);
      std::memcpy(dst->column(col) + dstRow * sz,
                  src->column(static_cast<int32_t>(i)) + slot.row * sz, sz);
    }

    // construct the new component
    auto &info = componentInfo_[componentId<T>()];
    new (dst->column(dst->indexOf(componentId<T>())) + dstRow * info.size)
        T(std::forward<Args>(args)...);

    // remove from src
    int32_t oldRow = slot.row;
    Entity moved = src->swapRemove(oldRow);
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

    // copy all except the removed component
    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      if (cid == componentId<T>())
        continue;
      size_t sz = componentInfo_[cid].size;
      auto col = dst->indexOf(cid);
      std::memcpy(dst->column(col) + dstRow * sz,
                  src->column(static_cast<int32_t>(i)) + slot.row * sz, sz);
    }

    // destruct removed component
    auto &info = componentInfo_[componentId<T>()];
    auto remCol = src->indexOf(componentId<T>());
    if (info.destruct)
      info.destruct(src->column(remCol) + slot.row * info.size);

    int32_t oldRow = slot.row;
    Entity moved = src->swapRemove(oldRow);
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
    return *(T *)(slot.archetype->column(col) + slot.row * info.size);
  }

  template <typename... Ts> auto view() {
    (registerIfNew<Ts>(), ...);

    View<Ts...> v;
    for (auto &[sig, arch] : archetypes_) {
      if (((arch.indexOf(componentId<Ts>()) != -1) && ...))
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
      void *comp = arch->column(i) + slot.row * info.size;
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
    return componentInfo_.emplace(id, ComponentInfo{sz, dtor, mv})
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
  std::map<std::vector<ComponentId>, Archetype> archetypes_;
  Archetype *rootArchetype_ = nullptr;
  std::unordered_map<ComponentId, ComponentInfo> componentInfo_;
};

} // namespace Raiden::Core
