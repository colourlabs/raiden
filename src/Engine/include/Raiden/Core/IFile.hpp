#pragma once

#include <cstddef>

namespace Raiden::Core {

class IFile {
public:
  IFile() = default;
  IFile(const IFile&) = delete;
  IFile& operator=(const IFile&) = delete;
  IFile(IFile&&) = delete;
  IFile& operator=(IFile&&) = delete;
  
  virtual ~IFile() = default;

  virtual size_t read(void *dst, size_t size) = 0;
  [[nodiscard]] virtual size_t size() const = 0;
  virtual bool seek(long offset, int origin) = 0; // SEEK_SET, SEEK_CUR, SEEK_END
  virtual void close() = 0;
};

} // namespace Raiden::Core
