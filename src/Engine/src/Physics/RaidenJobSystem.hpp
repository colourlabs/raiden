#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemThreadPool.h>

#include <memory>
#include <cstdint>

namespace Raiden::Physics {

class RaidenJobSystem {
public:
  explicit RaidenJobSystem(uint32_t maxConcurrency);
  ~RaidenJobSystem();

  RaidenJobSystem(const RaidenJobSystem &) = delete;
  RaidenJobSystem &operator=(const RaidenJobSystem &) = delete;
  RaidenJobSystem(RaidenJobSystem &&) = delete;
  RaidenJobSystem &operator=(RaidenJobSystem &&) = delete;

  [[nodiscard]] JPH::JobSystem *get() const { return impl_.get(); }

private:
  std::unique_ptr<JPH::JobSystemThreadPool> impl_;
};

} // namespace Raiden::Physics
