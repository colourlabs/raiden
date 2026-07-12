#pragma once

#include <Raiden/Physics/IPhysicsSystem.hpp>

#include <memory>

namespace Raiden::Physics {

class JoltPhysicsSystem : public IPhysicsSystem {
public:
  JoltPhysicsSystem();
  ~JoltPhysicsSystem() override;

  JoltPhysicsSystem(const JoltPhysicsSystem &) = delete;
  JoltPhysicsSystem &operator=(const JoltPhysicsSystem &) = delete;
  JoltPhysicsSystem(JoltPhysicsSystem &&) = delete;
  JoltPhysicsSystem &operator=(JoltPhysicsSystem &&) = delete;

  bool init(const PhysicsConfig &config) override;
  void shutdown() override;
  void step(float dt) override;

  void setGravity(const glm::vec3 &g) override;
  [[nodiscard]] glm::vec3 gravity() const override;
  void setFixedTimestep(float dt) override;
  [[nodiscard]] float fixedTimestep() const override;
  void setSolverPositions(uint32_t n) override;
  void setSolverVelocities(uint32_t n) override;

  [[nodiscard]] RaycastHit raycast(const glm::vec3 &origin,
                                   const glm::vec3 &direction,
                                   float maxDistance) const override;

  uint32_t createStaticBody(const glm::vec3 &pos,
                            const glm::quat &rot) override;
  uint32_t createDynamicBody(const glm::vec3 &pos, const glm::quat &rot,
                             float mass) override;
  uint32_t createKinematicBody(const glm::vec3 &pos,
                               const glm::quat &rot) override;
  void destroyBody(uint32_t bodyId) override;

  void setBodyTransform(uint32_t bodyId, const glm::vec3 &pos,
                        const glm::quat &rot) override;
  void getBodyTransform(uint32_t bodyId, glm::vec3 &pos,
                        glm::quat &rot) const override;
  void applyImpulse(uint32_t bodyId, const glm::vec3 &impulse) override;

  void addBoxCollider(uint32_t bodyId,
                      const glm::vec3 &halfExtents) override;
  void addSphereCollider(uint32_t bodyId, float radius) override;
  void addCapsuleCollider(uint32_t bodyId, float radius,
                          float height) override;

  [[nodiscard]] std::span<const ContactEvent> contacts() const override;
  void clearContacts() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace Raiden::Physics
