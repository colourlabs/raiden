#pragma once

#include <Raiden/Physics/ContactEvent.hpp>
#include <Raiden/Physics/PhysicsConfig.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <span>

namespace Raiden::Physics {

static constexpr uint32_t kNoBody = UINT32_MAX;

struct RaycastHit {
  uint32_t entity = kNoBody;
  glm::vec3 point{0.0F};
  glm::vec3 normal{0.0F};
  float distance = 0.0F;
};

class IPhysicsSystem {
public:
  IPhysicsSystem() = default;
  IPhysicsSystem(const IPhysicsSystem &) = delete;
  IPhysicsSystem &operator=(const IPhysicsSystem &) = delete;
  IPhysicsSystem(IPhysicsSystem &&) = delete;
  IPhysicsSystem &operator=(IPhysicsSystem &&) = delete;
  virtual ~IPhysicsSystem() = default;

  virtual bool init(const PhysicsConfig &config) = 0;
  virtual void shutdown() = 0;
  virtual void step(float dt) = 0;

  virtual void setGravity(const glm::vec3 &g) = 0;
  [[nodiscard]] virtual glm::vec3 gravity() const = 0;
  virtual void setFixedTimestep(float dt) = 0;
  [[nodiscard]] virtual float fixedTimestep() const = 0;
  virtual void setSolverPositions(uint32_t n) = 0;
  virtual void setSolverVelocities(uint32_t n) = 0;

  [[nodiscard]] virtual RaycastHit raycast(const glm::vec3 &origin,
                                           const glm::vec3 &direction,
                                           float maxDistance) const = 0;

  virtual uint32_t createStaticBody(const glm::vec3 &pos,
                                    const glm::quat &rot) = 0;
  virtual uint32_t createDynamicBody(const glm::vec3 &pos,
                                     const glm::quat &rot, float mass) = 0;
  virtual uint32_t createKinematicBody(const glm::vec3 &pos,
                                       const glm::quat &rot) = 0;
  virtual void destroyBody(uint32_t bodyId) = 0;

  virtual void setBodyTransform(uint32_t bodyId, const glm::vec3 &pos,
                                const glm::quat &rot) = 0;
  virtual void getBodyTransform(uint32_t bodyId, glm::vec3 &pos,
                                glm::quat &rot) const = 0;
  virtual void applyImpulse(uint32_t bodyId, const glm::vec3 &impulse) = 0;

  virtual void addBoxCollider(uint32_t bodyId,
                              const glm::vec3 &halfExtents) = 0;
  virtual void addSphereCollider(uint32_t bodyId, float radius) = 0;
  virtual void addCapsuleCollider(uint32_t bodyId, float radius,
                                  float height) = 0;

  [[nodiscard]] virtual std::span<const ContactEvent> contacts() const = 0;
  virtual void clearContacts() = 0;
};

} // namespace Raiden::Physics
