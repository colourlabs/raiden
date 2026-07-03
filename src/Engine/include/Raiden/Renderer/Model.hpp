#pragma once

#include <Raiden/Renderer/Mesh.hpp>

#include <algorithm>
#include <vector>

namespace Raiden::Renderer {

struct Model {
  std::vector<Mesh> meshes;

  bool isValid() const {
    return !meshes.empty() &&
           std::all_of(meshes.begin(), meshes.end(),
                       [](const Mesh &m) { return m.isValid(); });
  }
};

} // namespace Raiden::Renderer
