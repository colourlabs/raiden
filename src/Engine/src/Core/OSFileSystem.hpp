#pragma once

#include <Raiden/Core/IVirtualFileSystem.hpp>

#include <cstdio>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Raiden::Core {

struct MountPoint {
  std::string virtualPrefix;
  std::string realPath;
};

// IFile backed by an in-memory byte vector
class MemFile final : public IFile {
public:
  explicit MemFile(std::vector<std::byte> data);
  size_t read(void *dst, size_t size) override;
  [[nodiscard]] size_t size() const override;
  bool seek(long offset, int origin) override;
  void close() override;

private:
  std::vector<std::byte> data_;
  size_t pos_ = 0;
};

class OSFile final : public IFile {
public:
  explicit OSFile(const std::string &path);
  ~OSFile() override;

  OSFile(const OSFile&) = delete;
  OSFile& operator=(const OSFile&) = delete;

  OSFile(OSFile&& other) noexcept;
  OSFile& operator=(OSFile&& other) noexcept;

  size_t read(void *dst, size_t size) override;
  [[nodiscard]] size_t size() const override;
  bool seek(long offset, int origin) override;
  void close() override;

private:
  std::FILE *fp_ = nullptr;
  size_t fileSize_ = 0;
};

class OSFileSystem final : public IVirtualFileSystem {
public:
  bool mount(std::string_view virtualPath, std::string_view realPath) override;
  void registerData(std::string_view path,
                    std::vector<std::byte> data) override;
  std::unique_ptr<IFile> open(std::string_view path) override;
  bool exists(std::string_view path) const override;
  std::string readAll(std::string_view path) override;
  std::vector<std::byte> readBytes(std::string_view path) override;

  std::string resolveToRealPath(std::string_view virtualPath) const;

private:
  mutable std::shared_mutex mutex_;
  std::vector<MountPoint> mounts_;
  std::unordered_map<std::string, std::vector<std::byte>> memData_;
};

} // namespace Raiden::Core
