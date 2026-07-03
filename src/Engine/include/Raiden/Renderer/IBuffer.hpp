#pragma once

#include <cstddef>

namespace Raiden::Renderer {

class IBuffer {
public:
  virtual ~IBuffer() = default;
  virtual void upload(const void *data, size_t size) = 0;
  virtual size_t size() const = 0;

  virtual void *map() = 0;
  virtual void unmap() = 0;
};

} // namespace Raiden::Renderer