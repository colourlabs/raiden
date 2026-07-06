#pragma once

#include <Raiden/Renderer/RenderTypes.hpp>

#include <cstddef>
#include <cstdint>

namespace Raiden::Renderer {

class ITexture {
public:
  ITexture() = default;
  
  ITexture(const ITexture&) = delete;
  ITexture& operator=(const ITexture&) = delete;
  ITexture(ITexture&&) = delete;
  ITexture& operator=(ITexture&&) = delete;

  virtual ~ITexture() = default;
  virtual void upload(const void *data, size_t size) = 0;

  [[nodiscard]] virtual uint32_t getWidth() const = 0;
  [[nodiscard]] virtual uint32_t getHeight() const = 0;
  [[nodiscard]] virtual Format getFormat() const = 0;
  [[nodiscard]] virtual TextureType getType() const = 0;
  [[nodiscard]] virtual uint32_t getMipLevels() const = 0;
};

} // namespace Raiden::Renderer
