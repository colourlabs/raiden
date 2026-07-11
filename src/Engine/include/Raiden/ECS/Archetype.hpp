#pragma once

#include "Entity.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Raiden::ECS {

struct Archetype {
  static constexpr uint32_t kChunkCapacity = 64;

  uint64_t mask = 0;
  std::vector<ComponentId> signature;
  std::vector<size_t> componentSizes;
  std::vector<Entity> entities;

  // columns_[col] is a vector of chunks
  // each Chunk owns kChunkCapacity * componentSizes[col] bytes
  // entity at global row r: chunk = r / kChunkCapacity, offset = r %
  // kChunkCapacity
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
    auto row = static_cast<int32_t>(entities.size());
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

  // swapRemove with callbacks
  // moveColumn(col, dstRow, srcRow) — moves one column from srcRow to dstRow
  // destructColumn(col, row) — destructs one column at the given row
  // Both are called with the ACTUAL row values (not data pointers), allowing
  // the caller to use World-level knowledge for proper move construction.
  template <typename FM, typename FD>
  Entity swapRemove(int32_t row, FM &&moveColumn, FD &&destructColumn) {
    int32_t last = static_cast<int32_t>(entities.size()) - 1;
    Entity moved{.index = UINT32_MAX, .generation = 0};

    if (row != last) {
      moved = entities[last];
      entities[row] = moved;

      for (size_t i = 0; i < columns_.size(); ++i) {
        std::forward<FM>(moveColumn)(static_cast<int32_t>(i), row, last);
      }

      for (size_t i = 0; i < columns_.size(); ++i) {
        std::forward<FD>(destructColumn)(static_cast<int32_t>(i), last);
      }
    }

    entities.pop_back();
    return moved;
  }

  // overload without callbacks (trivially-movable components)
  Entity swapRemove(int32_t row) {
    int32_t last = static_cast<int32_t>(entities.size()) - 1;
    Entity moved{.index = UINT32_MAX, .generation = 0};

    if (row != last) {
      moved = entities[last];
      entities[row] = moved;
      for (size_t i = 0; i < columns_.size(); ++i) {
        auto sz = componentSizes[i];
        std::memcpy(data(i, row), data(i, last), sz);
      }
    }

    entities.pop_back();
    return moved;
  }

  std::byte *data(size_t col, int32_t row) {
    auto r = static_cast<uint32_t>(row);
    return columns_[col][r / kChunkCapacity].data.data() +
           ((r % kChunkCapacity) * componentSizes[col]);
  }

  [[nodiscard]] int32_t indexOf(ComponentId id) const {
    auto it = std::ranges::lower_bound(signature, id);

    if (it != signature.end() && *it == id) {
      return static_cast<int32_t>(it - signature.begin());
    }

    return -1;
  }
};

} // namespace Raiden::ECS
