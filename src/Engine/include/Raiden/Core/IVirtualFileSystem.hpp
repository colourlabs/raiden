#pragma once

#include <Raiden/Core/IFile.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Raiden::Core {

class IVirtualFileSystem {
public:
  IVirtualFileSystem() = default;
  IVirtualFileSystem(const IVirtualFileSystem&) = delete;
  IVirtualFileSystem& operator=(const IVirtualFileSystem&) = delete;
  IVirtualFileSystem(IVirtualFileSystem&&) = default;
  IVirtualFileSystem& operator=(IVirtualFileSystem&&) = default;

  virtual ~IVirtualFileSystem() = default;

  virtual bool mount(std::string_view virtualPath, std::string_view realPath) = 0;
  virtual void registerData(std::string_view path, std::vector<std::byte> data) = 0;
  virtual std::unique_ptr<IFile> open(std::string_view path) = 0;
  [[nodiscard]] virtual bool exists(std::string_view path) const = 0;
  virtual std::string readAll(std::string_view path) = 0;
  virtual std::vector<std::byte> readBytes(std::string_view path) = 0;
};

// factory – the implementation lives in a platform-specific source file
std::unique_ptr<IVirtualFileSystem> createOSFileSystem();

} // namespace Raiden::Core
