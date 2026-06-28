#pragma once

#include <RaidenEngineCore/Core/IFile.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Raiden::Core {

class IVirtualFileSystem {
public:
  virtual ~IVirtualFileSystem() = default;

  virtual bool mount(std::string_view virtualPath, std::string_view realPath) = 0;
  virtual std::unique_ptr<IFile> open(std::string_view path) = 0;
  virtual bool exists(std::string_view path) const = 0;
  virtual std::string readAll(std::string_view path) = 0;
  virtual std::vector<std::byte> readBytes(std::string_view path) = 0;
};

// factory – the implementation lives in a platform-specific source file
std::unique_ptr<IVirtualFileSystem> createOSFileSystem();

} // namespace Raiden::Core
