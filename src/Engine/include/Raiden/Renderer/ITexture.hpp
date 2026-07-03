#pragma once

#include <Raiden/Renderer/RenderTypes.hpp>

#include <cstddef>
#include <cstdint>

namespace Raiden::Renderer {

class ITexture {
public:
  virtual ~ITexture() = default;
  virtual void upload(const void *data, size_t size) = 0;

  virtual uint32_t getWidth() const = 0;
  virtual uint32_t getHeight() const = 0;
  virtual Format getFormat() const = 0;
  virtual TextureType getType() const = 0;
  virtual uint32_t getMipLevels() const = 0;
};

} // namespace Raiden::Renderer
