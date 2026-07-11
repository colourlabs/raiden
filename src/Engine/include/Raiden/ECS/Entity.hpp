#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>

namespace Raiden::ECS {

struct Entity {
  uint32_t index = 0;
  uint32_t generation = 0;

  auto operator<=>(const Entity &) const = default;
};

inline constexpr Entity NullEntity{};

using ComponentId = std::type_index;

template <typename T> inline ComponentId componentId() {
  return std::type_index(typeid(T));
}

} // namespace Raiden::ECS

template <> struct std::hash<Raiden::ECS::Entity> {
  size_t operator()(Raiden::ECS::Entity e) const noexcept {
    return (static_cast<size_t>(e.index) << 32) | e.generation;
  }
};