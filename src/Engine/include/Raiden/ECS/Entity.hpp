#pragma once

#include <cstdint>
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

}; // namespace Raiden::ECS