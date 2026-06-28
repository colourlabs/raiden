#include "OSFileSystem.hpp"
#include <RaidenEngineCore/Logger.hpp>

#include <sys/stat.h>

#include <cstdio>
#include <filesystem>

namespace Raiden::Core {

static const Logger s_logger("Raiden::Core::OSFileSystem");

OSFile::OSFile(const std::string &path) {
  fp_ = std::fopen(path.c_str(), "rb");
  if (!fp_) {
    return;
  }

  if (std::fseek(fp_, 0, SEEK_END) != 0) {
    std::fclose(fp_);
    fp_ = nullptr;
    return;
  }
  fileSize_ = static_cast<size_t>(std::ftell(fp_));
  std::fseek(fp_, 0, SEEK_SET);
}

OSFile::~OSFile() { close(); }

size_t OSFile::read(void *dst, size_t size) {
  if (!fp_)
    return 0;
  return std::fread(dst, 1, size, fp_);
}

size_t OSFile::size() const { return fileSize_; }

bool OSFile::seek(long offset, int origin) {
  if (!fp_)
    return false;
  return std::fseek(fp_, offset, origin) == 0;
}

void OSFile::close() {
  if (fp_) {
    std::fclose(fp_);
    fp_ = nullptr;
  }
}

bool OSFileSystem::mount(std::string_view virtualPath,
                         std::string_view realPath) {
  // normalize: ensure virtual prefix ends with /
  std::string vp(virtualPath);
  if (!vp.empty() && vp.back() != '/')
    vp.push_back('/');

  std::string rp(realPath);
  // ensure real path exists
  if (!std::filesystem::is_directory(rp)) {
    s_logger.error("Mount target '{}' is not a directory or does not exist.",
                   rp);
    return false;
  }
  if (!rp.empty() && rp.back() != '/')
    rp.push_back('/');

  mounts_.push_back({std::move(vp), std::move(rp)});
  s_logger.info("Mounted '{}' -> '{}'", virtualPath, realPath);
  return true;
}

std::string OSFileSystem::resolveToRealPath(std::string_view path) const {
  // find the longest matching prefix
  const MountPoint *best = nullptr;
  size_t bestLen = 0;

  for (const auto &mp : mounts_) {
    if (path.substr(0, mp.virtualPrefix.size()) == mp.virtualPrefix &&
        mp.virtualPrefix.size() > bestLen) {
      best = &mp;
      bestLen = mp.virtualPrefix.size();
    }
  }

  if (!best) {
    return std::string(path);
  }

  std::string suffix(path.substr(bestLen));
  return best->realPath + suffix;
}

std::unique_ptr<IFile> OSFileSystem::open(std::string_view path) {
  std::string realPath = resolveToRealPath(path);
  auto file = std::make_unique<OSFile>(realPath);
  if (file->size() == 0 && !std::filesystem::exists(realPath)) {
    s_logger.error("File not found: '{}' (resolved: '{}')", path, realPath);
    return nullptr;
  }
  return file;
}

bool OSFileSystem::exists(std::string_view path) const {
  std::string realPath = resolveToRealPath(path);
  return std::filesystem::exists(realPath);
}

std::string OSFileSystem::readAll(std::string_view path) {
  auto file = open(path);
  if (!file)
    return {};

  size_t sz = file->size();
  std::string result(sz, '\0');
  if (sz > 0) {
    file->read(result.data(), sz);
  }
  return result;
}

std::unique_ptr<IVirtualFileSystem> createOSFileSystem() {
  return std::make_unique<OSFileSystem>();
}

} // namespace Raiden::Core
