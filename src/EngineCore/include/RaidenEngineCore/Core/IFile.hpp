#pragma once

#include <cstddef>

namespace Raiden::Core {

class IFile {
public:
  virtual ~IFile() = default;

  virtual size_t read(void *dst, size_t size) = 0;
  virtual size_t size() const = 0;
  virtual bool seek(long offset, int origin) = 0; // SEEK_SET, SEEK_CUR, SEEK_END
  virtual void close() = 0;
};

} // namespace Raiden::Core
