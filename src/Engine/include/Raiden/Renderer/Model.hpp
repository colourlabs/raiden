#pragma once

#include <Raiden/Renderer/Mesh.hpp>

#include <algorithm>
#include <vector>

namespace Raiden::Renderer {

struct Model {
  std::vector<Mesh> meshes;

  [[nodiscard]] bool isValid() const {
    return !meshes.empty() && std::ranges::all_of(meshes, [](const Mesh &m) {
      return m.isValid();
    });
  }
};

} // namespace Raiden::Renderer
