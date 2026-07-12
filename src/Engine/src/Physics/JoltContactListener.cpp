#include "JoltContactListener.hpp"

#include <Jolt/Physics/Body/Body.h>

namespace Raiden::Physics {

void JoltContactListener::OnContactAdded(const JPH::Body &body1,
                                           const JPH::Body &body2,
                                           const JPH::ContactManifold &manifold,
                                           JPH::ContactSettings &) {
  std::lock_guard lock(mutex_);
  events_.emplace_back(RawContactEvent::Type::Added, body1.GetID(),
                       body2.GetID(), manifold.mWorldSpaceNormal);
}

void JoltContactListener::OnContactPersisted(
    const JPH::Body &body1, const JPH::Body &body2,
    const JPH::ContactManifold &manifold, JPH::ContactSettings &) {
  std::lock_guard lock(mutex_);
  events_.emplace_back(RawContactEvent::Type::Persisted, body1.GetID(),
                       body2.GetID(), manifold.mWorldSpaceNormal);
}

void JoltContactListener::OnContactRemoved(
    const JPH::SubShapeIDPair &subShapePair) {
  std::lock_guard lock(mutex_);
  events_.emplace_back(RawContactEvent::Type::Removed,
                       subShapePair.GetBody1ID(), subShapePair.GetBody2ID());
}

std::vector<RawContactEvent> JoltContactListener::drain() {
  std::lock_guard lock(mutex_);
  return std::move(events_);
}

} // namespace Raiden::Physics
