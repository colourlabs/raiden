#pragma once

#include "Entity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace Raiden::Core {

struct Archetype {
  std::vector<ComponentId> signature;
  std::vector<size_t> componentSizes;
  std::vector<Entity> entities;
  std::vector<std::vector<std::byte>> columns;

  Archetype() = default;

  Archetype(std::vector<ComponentId> sig, std::vector<size_t> sizes)
      : signature(std::move(sig)), componentSizes(std::move(sizes)) {
    columns.resize(signature.size());
  }

  int32_t add(Entity e) {
    int32_t row = static_cast<int32_t>(entities.size());
    entities.push_back(e);
    for (size_t i = 0; i < columns.size(); ++i)
      columns[i].resize(columns[i].size() + componentSizes[i]);
    return row;
  }

  Entity swapRemove(int32_t row) {
    int32_t last = static_cast<int32_t>(entities.size()) - 1;
    Entity moved{};

    if (row != last) {
      moved = entities[last];
      entities[row] = moved;
      for (size_t i = 0; i < columns.size(); ++i) {
        auto &col = columns[i];
        size_t sz = componentSizes[i];
        std::memcpy(&col[row * sz], &col[last * sz], sz);
      }
    }

    entities.pop_back();
    for (size_t i = 0; i < columns.size(); ++i)
      columns[i].resize(columns[i].size() - componentSizes[i]);

    return moved;
  }

  std::byte *column(size_t i) { return columns[i].data(); }

  const std::byte *column(size_t i) const { return columns[i].data(); }

  int32_t indexOf(ComponentId id) const {
    auto it = std::lower_bound(signature.begin(), signature.end(), id);
    if (it != signature.end() && *it == id)
      return static_cast<int32_t>(std::distance(signature.begin(), it));
    return -1;
  }
};

} // namespace Raiden::Core
