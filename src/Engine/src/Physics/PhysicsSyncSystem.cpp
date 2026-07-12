#include <Raiden/Physics/PhysicsSyncSystem.hpp>

#include <Raiden/ECS/Collider.hpp>
#include <Raiden/ECS/Rigidbody.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Physics/IPhysicsSystem.hpp>

namespace Raiden::Physics {

PhysicsSyncSystem::PhysicsSyncSystem(IPhysicsSystem &physics)
    : physics_(physics) {}

ECS::Entity PhysicsSyncSystem::resolveEntity(uint32_t bodyId) const {
  auto it = bodyToEntity_.find(bodyId);
  if (it != bodyToEntity_.end()) {
    return it->second;
  }
  return ECS::NullEntity;
}

void PhysicsSyncSystem::update(ECS::World &world, float dt) {
  (void)dt;

  world.view<ECS::Transform, ECS::Rigidbody>().each(
      [&](ECS::Entity e, ECS::Transform &t, ECS::Rigidbody &rb) {
        if (rb.bodyId == UINT32_MAX) {
          if (rb.type == ECS::Rigidbody::Type::Static) {
            rb.bodyId = physics_.createStaticBody(t.translation, t.rotation);
          } else if (rb.type == ECS::Rigidbody::Type::Kinematic) {
            rb.bodyId =
                physics_.createKinematicBody(t.translation, t.rotation);
          } else {
            rb.bodyId =
                physics_.createDynamicBody(t.translation, t.rotation, rb.mass);
          }

          if (rb.bodyId != UINT32_MAX && world.has<ECS::Collider>(e)) {
            auto &col = world.get<ECS::Collider>(e);
            switch (col.shape) {
            case ECS::Collider::Shape::Box:
              physics_.addBoxCollider(rb.bodyId, col.halfExtents);
              break;
            case ECS::Collider::Shape::Sphere:
              physics_.addSphereCollider(rb.bodyId, col.radius);
              break;
            case ECS::Collider::Shape::Capsule:
              physics_.addCapsuleCollider(rb.bodyId, col.radius, col.height);
              break;
            }
          }

          if (rb.bodyId != UINT32_MAX) {
            bodyToEntity_[rb.bodyId] = e;
          }
        }

        if (rb.bodyId == UINT32_MAX) {
          return;
        }

        if (rb.type == ECS::Rigidbody::Type::Dynamic) {
          physics_.getBodyTransform(rb.bodyId, t.translation, t.rotation);
        } else if (rb.type == ECS::Rigidbody::Type::Kinematic) {
          physics_.setBodyTransform(rb.bodyId, t.translation, t.rotation);
        }
      });

  physics_.clearContacts();
}

} // namespace Raiden::Physics
