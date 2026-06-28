#pragma once

#include <cstddef>

namespace Raiden::Core {

class ITexture {
public:
  virtual ~ITexture() = default;
  virtual void upload(const void *data, size_t size) = 0;
};

} // namespace Raiden::Core
