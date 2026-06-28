#pragma once

#include <RaidenEngineCore/Core/IVirtualFileSystem.hpp>

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace Raiden::Core {

struct MountPoint {
  std::string virtualPrefix;
  std::string realPath;
};

class OSFile final : public IFile {
public:
  explicit OSFile(const std::string &path);
  ~OSFile() override;

  size_t read(void *dst, size_t size) override;
  size_t size() const override;
  bool seek(long offset, int origin) override;
  void close() override;

private:
  std::FILE *fp_ = nullptr;
  size_t fileSize_ = 0;
};

class OSFileSystem final : public IVirtualFileSystem {
public:
  bool mount(std::string_view virtualPath, std::string_view realPath) override;
  std::unique_ptr<IFile> open(std::string_view path) override;
  bool exists(std::string_view path) const override;
  std::string readAll(std::string_view path) override;

  std::string resolveToRealPath(std::string_view virtualPath) const;

private:
  std::vector<MountPoint> mounts_;
};

} // namespace Raiden::Core
