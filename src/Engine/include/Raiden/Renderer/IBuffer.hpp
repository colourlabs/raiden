#pragma once

#include <cstddef>

namespace Raiden::Renderer {

class IBuffer {
public:
  IBuffer() = default;
  IBuffer(const IBuffer&) = delete;
  IBuffer& operator=(const IBuffer&) = delete;
  IBuffer(IBuffer&&) = default;
  IBuffer& operator=(IBuffer&&) = default;
  virtual ~IBuffer() = default;

  virtual void upload(const void *data, size_t size) = 0;
  [[nodiscard]] virtual size_t size() const = 0;

  virtual void *map() = 0;
  virtual void unmap() = 0;
};

} // namespace Raiden::Renderer