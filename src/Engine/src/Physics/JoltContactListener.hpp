#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>

#include <mutex>
#include <vector>

namespace Raiden::Physics {

struct RawContactEvent {
  enum class Type : uint8_t { Added, Persisted, Removed };

  RawContactEvent(Type t, JPH::BodyID a, JPH::BodyID b,
                  JPH::Vec3 n = JPH::Vec3::sZero())
      : type(t), bodyA(a), bodyB(b), contactNormal(n) {}

  Type type;
  JPH::BodyID bodyA;
  JPH::BodyID bodyB;
  JPH::Vec3 contactNormal;
};

class JoltContactListener final : public JPH::ContactListener {
public:
  void OnContactAdded(const JPH::Body &body1, const JPH::Body &body2,
                      const JPH::ContactManifold &manifold,
                      JPH::ContactSettings &settings) override;

  void OnContactPersisted(const JPH::Body &body1, const JPH::Body &body2,
                          const JPH::ContactManifold &manifold,
                          JPH::ContactSettings &settings) override;

  void OnContactRemoved(const JPH::SubShapeIDPair &subShapePair) override;

  std::vector<RawContactEvent> drain();

private:
  std::mutex mutex_;
  std::vector<RawContactEvent> events_;
};

} // namespace Raiden::Physics
