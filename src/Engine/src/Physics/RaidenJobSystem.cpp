#include "RaidenJobSystem.hpp"

namespace Raiden::Physics {

RaidenJobSystem::RaidenJobSystem(uint32_t maxConcurrency)
    : impl_(std::make_unique<JPH::JobSystemThreadPool>(
          1024,      // inMaxJobs
          8,     // inMaxBarriers
          static_cast<int>(maxConcurrency))) {}

RaidenJobSystem::~RaidenJobSystem() = default;

} // namespace Raiden::Physics
