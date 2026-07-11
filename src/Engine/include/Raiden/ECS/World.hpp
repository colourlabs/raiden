#pragma once

#include "Archetype.hpp"
#include "Entity.hpp"
#include "System.hpp"

#include <cstring>
#include <memory>
#include <list>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Raiden::ECS {

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
    if constexpr (std::is_move_constructible_v<T>) {
      new (d) T(std::move(*s));
    } else {
      new (d) T(*s);
    }
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
          std::forward<F>(fn)(
              arch->entities[r],
              *(Ts *)(arch->data(cols[I], static_cast<int32_t>(r)))...);
        }(std::index_sequence_for<Ts...>{});
      }
    }
  }

  [[nodiscard]] size_t size() const {
    size_t total = 0;
    for (auto *a : archetypes) {
      total += a->entities.size();
    }
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

  World(World &&) noexcept = default;
  World &operator=(World &&) noexcept = default;

  Entity create();
  void destroy(Entity e);

  [[nodiscard]] uint32_t entityCount() const {
    return static_cast<uint32_t>(slots_.size());
  }

  template <typename T, typename... Args> T &assign(Entity e, Args &&...args) {
    registerIfNew<T>();
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation) {
      throw std::runtime_error("assign<T>() called with invalid Entity handle");
    }

    Archetype *src = slot.archetype;

    std::vector<ComponentId> sig = src->signature;
    auto it = std::lower_bound(sig.begin(), sig.end(), componentId<T>());
    if (it != sig.end() && *it == componentId<T>()) {
      return get<T>(e);
    }

    sig.insert(it, componentId<T>());

    Archetype *dst = findOrCreateArchetype(sig);
    int32_t dstRow =
        dst->add(Entity{.index = e.index, .generation = slot.generation});

    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      auto &cinfo = componentInfo_[cid];
      auto col = dst->indexOf(cid);
      cinfo.move(dst->data(col, dstRow),
                 src->data(static_cast<int32_t>(i), slot.row));
    }

    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      auto &cinfo = componentInfo_[cid];
      if (cinfo.destruct) {
        cinfo.destruct(src->data(static_cast<int32_t>(i), slot.row));
      }
    }

    auto dstCol = dst->indexOf(componentId<T>());
    T *ptr = new (dst->data(dstCol, dstRow)) T(std::forward<Args>(args)...);

    int32_t oldRow = slot.row;
    auto *srcArch = src;
    Entity moved = srcArch->swapRemove(
        oldRow,
        [&](int32_t ci, int32_t dstRow_, int32_t srcRow_) {
          auto cid = srcArch->signature[ci];
          auto &cinfo = componentInfo_[cid];
          cinfo.move(srcArch->data(ci, dstRow_),
                     srcArch->data(ci, srcRow_));
        },
        [&](int32_t ci, int32_t r) {
          auto cid = srcArch->signature[ci];
          auto &cinfo = componentInfo_[cid];
          if (cinfo.destruct) {
            cinfo.destruct(srcArch->data(ci, r));
          }
        });
    if (moved.index != uint32_t(-1)) {
      slots_[moved.index].row = oldRow;
    }

    slot.archetype = dst;
    slot.row = dstRow;

    return *ptr;
  }

  template <typename T> void remove(Entity e) {
    registerIfNew<T>();
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation) {
      return;
    }

    Archetype *src = slot.archetype;

    std::vector<ComponentId> sig;
    for (auto id : src->signature) {
      if (id != componentId<T>()) {
        sig.push_back(id);
      }
    }

    if (sig.size() == src->signature.size()) {
      return; // didn't have it
    }

    Archetype *dst = findOrCreateArchetype(sig);
    int32_t dstRow =
        dst->add(Entity{.index = e.index, .generation = slot.generation});

    // move all except the removed component
    for (size_t i = 0; i < src->signature.size(); ++i) {
      auto cid = src->signature[i];
      if (cid == componentId<T>()) {
        continue;
      }
      auto &cinfo = componentInfo_[cid];
      auto col = dst->indexOf(cid);
      cinfo.move(dst->data(col, dstRow),
                 src->data(static_cast<int32_t>(i), slot.row));
    }

    // destruct removed component
    auto &rinfo = componentInfo_[componentId<T>()];
    auto remCol = src->indexOf(componentId<T>());
    if (rinfo.destruct) {
      rinfo.destruct(src->data(remCol, slot.row));
    }

    // remove from src (moves last-row components then destructs)
    int32_t oldRow = slot.row;
    auto *srcArch = src;
    Entity moved = srcArch->swapRemove(
        oldRow,
        [&](int32_t ci, int32_t dstRow_, int32_t srcRow_) {
          auto cid = srcArch->signature[ci];
          auto &cinfo = componentInfo_[cid];
          cinfo.move(srcArch->data(ci, dstRow_),
                     srcArch->data(ci, srcRow_));
        },
        [&](int32_t ci, int32_t r) {
          auto cid = srcArch->signature[ci];
          auto &cinfo = componentInfo_[cid];
          if (cinfo.destruct) {
            cinfo.destruct(srcArch->data(ci, r));
          }
        });
    if (moved.index != uint32_t(-1)) {
      slots_[moved.index].row = oldRow;
    }

    slot.archetype = dst;
    slot.row = dstRow;
  }

  template <typename T> T &get(Entity e) {
    auto &slot = slots_[e.index];
    (void)slot.generation;
    [[maybe_unused]] auto &info = registerIfNew<T>();
    auto col = slot.archetype->indexOf(componentId<T>());
    return *(T *)(slot.archetype->data(col, slot.row));
  }

  template <typename... Ts> auto view() {
    (registerIfNew<Ts>(), ...);

    uint64_t queryMask = 0;
    ((queryMask |= (uint64_t(1) << componentInfo_[componentId<Ts>()].bitIndex)),
     ...);

    View<Ts...> v;
    for (auto &arch : archetypes_) {
      if ((arch.mask & queryMask) == queryMask) {
        v.archetypes.push_back(&arch);
      }
    }
    return v;
  }

  // set a human-readable name for a component type (used by forEachComponent)
  template <typename T> void registerComponent(std::string_view name) {
    registerIfNew<T>().name = name;
  }

  template <typename T, typename... Args> T &addSystem(Args &&...args) {
    auto system = std::make_unique<T>(std::forward<Args>(args)...);
    T &ref = *system;
    systems_.push_back(std::move(system));
    return ref;
  }

  void updateSystems(float dt) {
    for (auto &sys : systems_) {
      sys->update(*this, dt);
    }
  }

  void renderSystems() {
    for (auto &sys : systems_) {
      sys->render(*this);
    }
  }

  // iterate all components on an entity without knowing types at compile time
  // visitor receives (Entity, ComponentId, std::string_view name, void *data)
  template <typename F> void forEachComponent(Entity e, F &&visitor) {
    auto &slot = slots_[e.index];
    if (slot.generation != e.generation) {
      return;
    }

    auto *arch = slot.archetype;
    for (size_t i = 0; i < arch->signature.size(); ++i) {
      auto cid = arch->signature[i];
      auto &info = componentInfo_[cid];
      void *comp = arch->data(static_cast<int32_t>(i), slot.row);
      std::forward<F>(visitor)(e, cid, info.name, comp);
    }
  }

private:
  ComponentInfo &ensureComponent(ComponentId id, size_t sz,
                                 void (*dtor)(void *) = nullptr,
                                 void (*mv)(void *, void *) = nullptr) {
    auto it = componentInfo_.find(id);

    if (it != componentInfo_.end()) {
      return it->second;
    }

    uint32_t bit = nextComponentBit_++;
    return componentInfo_
        .emplace(id, ComponentInfo{.size = sz,
                                   .destruct = dtor,
                                   .move = mv,
                                   .name = {},
                                   .bitIndex = bit})
        .first->second;
  }

  // register Ts... in view() if not already known
  template <typename T> ComponentInfo &registerIfNew() {
    return ensureComponent(componentId<T>(), sizeof(T), componentDestruct<T>,
                           componentMove<T>);
  }
  Archetype *findOrCreateArchetype(const std::vector<ComponentId> &sig);

  Archetype &rootArchetype();
  Archetype *lookupArchetype(uint64_t mask);
  Archetype &createArchetype(uint64_t mask, std::vector<ComponentId> sig,
                             std::vector<size_t> sizes);

  std::vector<Slot> slots_;
  uint32_t freeHead_ = uint32_t(-1);
  std::list<Archetype> archetypes_;
  std::unordered_map<uint64_t, Archetype *> archetypeIndex_;
  uint32_t nextComponentBit_ = 0;
  std::unordered_map<ComponentId, ComponentInfo> componentInfo_;
  std::vector<std::unique_ptr<System>> systems_;
};

} // namespace Raiden::ECS
