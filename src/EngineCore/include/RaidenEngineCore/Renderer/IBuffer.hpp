#pragma once

#include <cstddef>

namespace Raiden::Core {

class IBuffer {
public:
  virtual ~IBuffer() = default;
  virtual void upload(const void *data, size_t size) = 0;
  virtual size_t size() const = 0;
};

} // namespace Raiden::Core