#pragma once

#include "Entity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace Raiden::ECS {

struct Archetype {
  static constexpr uint32_t kChunkCapacity = 64;

  uint64_t mask = 0;
  std::vector<ComponentId> signature;
  std::vector<size_t> componentSizes;
  std::vector<Entity> entities;

  // columns_[col] is a vector of chunks
  // each Chunk owns kChunkCapacity * componentSizes[col] bytes
  // entity at global row r: chunk = r / kChunkCapacity, offset = r % kChunkCapacity
  struct Chunk {
    std::vector<std::byte> data;
  };

  std::vector<std::vector<Chunk>> columns_;

  Archetype() = default;

  Archetype(uint64_t m, std::vector<ComponentId> sig, std::vector<size_t> sizes)
      : mask(m), signature(std::move(sig)), componentSizes(std::move(sizes)) {
    columns_.resize(signature.size());
  }

  int32_t add(Entity e) {
    int32_t row = static_cast<int32_t>(entities.size());
    uint32_t chunkIdx = static_cast<uint32_t>(row) / kChunkCapacity;

    for (size_t ci = 0; ci < columns_.size(); ++ci) {
      while (columns_[ci].size() <= chunkIdx) {
        auto &chunk = columns_[ci].emplace_back();
        chunk.data.resize(kChunkCapacity * componentSizes[ci]);
      }
    }

    entities.push_back(e);
    return row;
  }

  // swapRemove with callback to destruct last-row components after memcpy
  template <typename F>
  Entity swapRemove(int32_t row, F &&destructLast) {
    int32_t last = static_cast<int32_t>(entities.size()) - 1;
    Entity moved{};

    if (row != last) {
      moved = entities[last];
      entities[row] = moved;
      for (size_t i = 0; i < columns_.size(); ++i) {
        auto sz = componentSizes[i];
        std::memcpy(data(i, row), data(i, last), sz);
      }
      destructLast(last);
    }

    entities.pop_back();
    return moved;
  }

  // overload without callback (for callers that pre-destructed)
  Entity swapRemove(int32_t row) {
    return swapRemove(row, [](int32_t) {});
  }

  std::byte *data(size_t col, int32_t row) {
    uint32_t r = static_cast<uint32_t>(row);
    return columns_[col][r / kChunkCapacity].data.data() +
           (r % kChunkCapacity) * componentSizes[col];
  }

  int32_t indexOf(ComponentId id) const {
    auto it = std::lower_bound(signature.begin(), signature.end(), id);
    if (it != signature.end() && *it == id)
      return static_cast<int32_t>(std::distance(signature.begin(), it));
    return -1;
  }
};

} // namespace Raiden::ECS
