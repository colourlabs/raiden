#include "OSFileSystem.hpp"
#include <Raiden/Logger.hpp>

#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::OSFileSystem");

// MemFile

MemFile::MemFile(std::vector<std::byte> data) : data_(std::move(data)) {}

size_t MemFile::read(void *dst, size_t size) {
  size_t avail = data_.size() - pos_;
  size_t toRead = std::min(size, avail);
  if (toRead > 0) {
    std::memcpy(dst, data_.data() + pos_, toRead);
    pos_ += toRead;
  }
  return toRead;
}

size_t MemFile::size() const { return data_.size(); }

bool MemFile::seek(long offset, int origin) {
  switch (origin) {
  case SEEK_SET:
    pos_ = static_cast<size_t>(offset);
    break;
  case SEEK_CUR:
    pos_ = static_cast<size_t>(static_cast<long>(pos_) + offset);
    break;
  case SEEK_END:
    pos_ = static_cast<size_t>(static_cast<long>(data_.size()) + offset);
    break;
  default:
    return false;
  }

  pos_ = std::min(pos_, data_.size());
  return true;
}

void MemFile::close() {
  data_.clear();
  pos_ = 0;
}

// OSFile

OSFile::OSFile(const std::string &path) : fp_(std::fopen(path.c_str(), "rb")) {
  if (fp_ == nullptr) {
    return;
  }

  if (std::fseek(fp_, 0, SEEK_END) != 0) {
    std::fclose(fp_);
    fp_ = nullptr;
    return;
  }

  auto pos = std::ftell(fp_);
  if (pos < 0) {
    close();
    return;
  }

  fileSize_ = static_cast<size_t>(pos);
}

OSFile::~OSFile() { close(); }

size_t OSFile::read(void *dst, size_t size) {
  if (fp_ == nullptr) {
    return 0;
  }

  return std::fread(dst, 1, size, fp_);
}

size_t OSFile::size() const { return fileSize_; }

bool OSFile::seek(long offset, int origin) {
  if (fp_ == nullptr) {
    return false;
  }

  return std::fseek(fp_, offset, origin) == 0;
}

void OSFile::close() {
  if (fp_ != nullptr) {
    std::fclose(fp_);
    fp_ = nullptr;
  }
}

// OSFileSystem

void OSFileSystem::registerData(std::string_view path,
                                std::vector<std::byte> data) {
  std::scoped_lock lock(mutex_);
  memData_[std::string(path)] = std::move(data);
}

std::unique_ptr<IFile> OSFileSystem::open(std::string_view path) {
  std::shared_lock lock(mutex_);

  // check in-memory data first
  {
    auto it = memData_.find(std::string(path));
    if (it != memData_.end()) {
      return std::make_unique<MemFile>(it->second);
    }
  }

  std::string realPath = resolveToRealPath(path);
  lock.unlock();

  auto file = std::make_unique<OSFile>(realPath);
  if (file->size() == 0 && !std::filesystem::exists(realPath)) {
    s_logger.error("File not found: '{}' (resolved: '{}')", path, realPath);
    return nullptr;
  }
  return file;
}

bool OSFileSystem::exists(std::string_view path) const {
  std::shared_lock lock(mutex_);

  if (memData_.contains(std::string(path))) {
    return true;
  }

  std::string realPath = resolveToRealPath(path);
  lock.unlock();

  return std::filesystem::exists(realPath);
}

std::string OSFileSystem::readAll(std::string_view path) {
  // fast path: in-memory data
  {
    std::shared_lock lock(mutex_);
    auto it = memData_.find(std::string(path));
    if (it != memData_.end()) {
      auto data = it->second;
      return {reinterpret_cast<const char *>(data.data()),
              data.size()};
    }
  }

  auto file = open(path);
  if (!file) {
    return {};
  }

  size_t sz = file->size();
  std::string result(sz, '\0');
  if (sz > 0) {
    file->read(result.data(), sz);
  }
  return result;
}

std::vector<std::byte> OSFileSystem::readBytes(std::string_view path) {
  // fast path: in-memory data (returns a copy)
  {
    std::shared_lock lock(mutex_);
    auto it = memData_.find(std::string(path));
    if (it != memData_.end()) {
      return it->second;
    }
  }

  auto file = open(path);
  if (!file) {
    return {};
  }

  size_t sz = file->size();
  std::vector<std::byte> result(sz);
  if (sz > 0) {
    file->read(result.data(), sz);
  }
  return result;
}

bool OSFileSystem::mount(std::string_view virtualPath,
                         std::string_view realPath) {
  // normalize: ensure virtual prefix ends with /
  std::string vp(virtualPath);
  if (!vp.empty() && vp.back() != '/') {
    vp.push_back('/');
  }

  std::string rp(realPath);
  // ensure real path exists
  if (!std::filesystem::is_directory(rp)) {
    s_logger.error("Mount target '{}' is not a directory or does not exist.",
                   rp);
    return false;
  }

  if (!rp.empty() && rp.back() != '/') {
    rp.push_back('/');
  }

  {
    std::scoped_lock lock(mutex_);
    mounts_.push_back({std::move(vp), std::move(rp)});
  }
  s_logger.info("Mounted '{}' -> '{}'", virtualPath, realPath);
  return true;
}

std::string OSFileSystem::resolveToRealPath(std::string_view path) const {
  // caller must hold mutex_ (shared or exclusive)
  // find the longest matching prefix
  const MountPoint *best = nullptr;
  size_t bestLen = 0;

  for (const auto &mp : mounts_) {
    if (path.starts_with(mp.virtualPrefix) &&
        mp.virtualPrefix.size() > bestLen) {
      best = &mp;
      bestLen = mp.virtualPrefix.size();
    }
  }

  if (best == nullptr) {
    return std::string(path);
  }

  std::string suffix(path.substr(bestLen));
  return best->realPath + suffix;
}

std::unique_ptr<IVirtualFileSystem> createOSFileSystem() {
  return std::make_unique<OSFileSystem>();
}

} // namespace Raiden::Core
