#pragma once

#include <Raiden/Renderer/IBuffer.hpp>
#include <Raiden/Renderer/IMaterial.hpp>

#include <cstdint>
#include <memory>

namespace Raiden::Renderer {

struct Mesh {
  std::shared_ptr<IBuffer> vertexBuffer;
  std::shared_ptr<IBuffer> indexBuffer;
  uint32_t indexCount = 0;
  uint32_t indexOffset = 0;
  std::shared_ptr<IMaterial> material;

  bool isValid() const { return vertexBuffer && indexBuffer && indexCount > 0; }
};

} // namespace Raiden::Renderer