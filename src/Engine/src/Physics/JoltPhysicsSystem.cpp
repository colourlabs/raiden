#include "JoltPhysicsSystem.hpp"
#include "JoltContactListener.hpp"
#include "RaidenJobSystem.hpp"

#include <Raiden/ECS/Rigidbody.hpp>
#include <Raiden/Logger.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <unordered_map>

static const ::Raiden::Core::Logger s_logger("Raiden::Physics");

namespace Raiden::Physics {

static JPH::Vec3 toJolt(const glm::vec3 &v) {
  return {v.x, v.y, v.z};
}

static glm::vec3 toGlm(JPH::Vec3 v) {
  return {v.GetX(), v.GetY(), v.GetZ()};
}

static JPH::Quat toJolt(const glm::quat &q) {
  return {q.x, q.y, q.z, q.w};
}

static glm::quat toGlm(JPH::Quat q) {
  return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
}

class BroadPhaseLayerImpl final : public JPH::BroadPhaseLayerInterface {
public:
  [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
      JPH::ObjectLayer inLayer) const override {
    switch (inLayer) {
    case 0:
      return JPH::BroadPhaseLayer(0);
    case 1:
      return JPH::BroadPhaseLayer(1);
    default:
      return JPH::BroadPhaseLayer(0);
    }
  }

  [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  [[nodiscard]] const char *GetBroadPhaseLayerName(
      JPH::BroadPhaseLayer inLayer) const override {
    switch (inLayer.GetValue()) {
    case 0:
      return "Static";
    case 1:
      return "Dynamic";
    default:
      return "Unknown";
    }
  }
#endif
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
  [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inObject1,
                     JPH::ObjectLayer inObject2) const override {
    if (inObject1 == inObject2) {
      return true;
    }
    return inObject1 <= 1 && inObject2 <= 1;
  }
};

class ObjectVsBroadPhaseLayerFilterImpl final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inObject1,
                     JPH::BroadPhaseLayer inObject2) const override {
    (void)inObject1;
    (void)inObject2;
    return true;
  }
};

class ActivationListenerImpl final : public JPH::BodyActivationListener {
public:
  void OnBodyActivated(const JPH::BodyID & /*inBodyID*/, JPH::uint64 /*inBodyUserData*/) override {}
  void OnBodyDeactivated(const JPH::BodyID & /*inBodyID*/, JPH::uint64 /*inBodyUserData*/) override {}
};

struct JoltPhysicsSystem::Impl {
  PhysicsConfig config;
  float accumulator = 0.0F;

  std::unique_ptr<RaidenJobSystem> jobSystem;
  std::unique_ptr<JPH::Factory> factory;
  std::unique_ptr<JPH::TempAllocatorMalloc> tempAllocator;
  std::unique_ptr<JPH::PhysicsSystem> physics;
  JPH::BodyInterface *bodyInterface = nullptr;

  ActivationListenerImpl activationListener;
  JoltContactListener contactListener;
  BroadPhaseLayerImpl broadPhaseImpl;
  ObjectLayerPairFilterImpl objectLayerFilterImpl;
  ObjectVsBroadPhaseLayerFilterImpl broadPhaseFilterImpl;

  uint32_t nextBodyId = 0;
  std::unordered_map<uint32_t, JPH::BodyID> bodyMap;
  std::unordered_map<JPH::BodyID, uint32_t> reverseMap;

  std::vector<ContactEvent> contacts;

  static constexpr JPH::ObjectLayer kStaticLayer = 0;
  static constexpr JPH::ObjectLayer kDynamicLayer = 1;
};

JoltPhysicsSystem::JoltPhysicsSystem() = default;
JoltPhysicsSystem::~JoltPhysicsSystem() { shutdown(); }

bool JoltPhysicsSystem::init(const PhysicsConfig &config) {
  impl_ = std::make_unique<Impl>();
  impl_->config = config;

  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  impl_->factory.reset(JPH::Factory::sInstance);

  JPH::RegisterTypes();

  uint32_t hwThreads = std::thread::hardware_concurrency();
  uint32_t physicsThreads = std::max(1U, hwThreads > 1 ? hwThreads / 2 : 1);
  impl_->jobSystem = std::make_unique<RaidenJobSystem>(physicsThreads);

  impl_->tempAllocator = std::make_unique<JPH::TempAllocatorMalloc>();

  impl_->physics = std::make_unique<JPH::PhysicsSystem>();
  impl_->physics->Init(config.maxBodies, 0, config.maxBodyPairs,
                       config.maxContactConstraints, impl_->broadPhaseImpl,
                       impl_->broadPhaseFilterImpl,
                       impl_->objectLayerFilterImpl);

  impl_->physics->SetBodyActivationListener(&impl_->activationListener);
  impl_->physics->SetContactListener(&impl_->contactListener);

  impl_->bodyInterface = &impl_->physics->GetBodyInterface();

  impl_->physics->SetGravity(toJolt(config.gravity));

  s_logger.info("Physics initialized ({} threads, gravity [{}, {}, {}])",
                physicsThreads, config.gravity.x, config.gravity.y,
                config.gravity.z);

  return true;
}

void JoltPhysicsSystem::shutdown() {
  if (!impl_) {
    return;
  }

  impl_->physics.reset();
  impl_->bodyInterface = nullptr;
  impl_->tempAllocator.reset();
  impl_->jobSystem.reset();

  JPH::UnregisterTypes();
  JPH::Factory::sInstance = nullptr;

  impl_.reset();
}

void JoltPhysicsSystem::step(float dt) {
  if (!impl_) {
    return;
  }

  impl_->accumulator += dt;
  while (impl_->accumulator >= impl_->config.fixedTimestep) {
    impl_->physics->Update(impl_->config.fixedTimestep, 1,
                           impl_->tempAllocator.get(),
                           impl_->jobSystem->get());
    impl_->accumulator -= impl_->config.fixedTimestep;
  }

  auto raw = impl_->contactListener.drain();
  for (auto &evt : raw) {
    ContactEvent contact;
    switch (evt.type) {
    case RawContactEvent::Type::Added:
      contact.type = ContactEvent::Type::Added;
      break;
    case RawContactEvent::Type::Persisted:
      contact.type = ContactEvent::Type::Persisted;
      break;
    case RawContactEvent::Type::Removed:
      contact.type = ContactEvent::Type::Removed;
      break;
    }
    contact.contactNormal = toGlm(evt.contactNormal);

    auto aIt = impl_->reverseMap.find(evt.bodyA);
    contact.bodyIdA =
        aIt != impl_->reverseMap.end() ? aIt->second : kNoBody;

    auto bIt = impl_->reverseMap.find(evt.bodyB);
    contact.bodyIdB =
        bIt != impl_->reverseMap.end() ? bIt->second : kNoBody;

    if (contact.bodyIdA != kNoBody && contact.bodyIdB != kNoBody) {
      impl_->contacts.push_back(contact);
    }
  }
}

void JoltPhysicsSystem::setGravity(const glm::vec3 &g) {
  if (!impl_) {
    return;
  }
  impl_->config.gravity = g;
  impl_->physics->SetGravity(toJolt(g));
}

glm::vec3 JoltPhysicsSystem::gravity() const {
  if (!impl_) {
    return {0.0F, -9.81F, 0.0F};
  }
  return toGlm(impl_->physics->GetGravity());
}

void JoltPhysicsSystem::setFixedTimestep(float dt) {
  if (impl_) {
    impl_->config.fixedTimestep = dt;
  }
}

float JoltPhysicsSystem::fixedTimestep() const {
  return impl_ ? impl_->config.fixedTimestep : 1.0F / 60.0F;
}

void JoltPhysicsSystem::setSolverPositions(uint32_t n) {
  if (impl_) {
    impl_->config.solverPositions = n;
  }
}

void JoltPhysicsSystem::setSolverVelocities(uint32_t n) {
  if (impl_) {
    impl_->config.solverVelocities = n;
  }
}

RaycastHit JoltPhysicsSystem::raycast(const glm::vec3 &origin,
                                       const glm::vec3 &direction,
                                       float maxDistance) const {
  RaycastHit hit;
  if (!impl_) {
    return hit;
  }

  JPH::RRayCast ray{toJolt(origin), toJolt(direction * maxDistance)};
  JPH::RayCastResult result;

  if (impl_->physics->GetNarrowPhaseQuery().CastRay(ray, result)) {
    hit.distance = result.mFraction * maxDistance;
    hit.point = toGlm(ray.mOrigin + ray.mDirection * result.mFraction);
    hit.normal = toGlm(
        impl_->bodyInterface->GetRotation(result.mBodyID)
            * JPH::Vec3(0, 1, 0));

    auto it = impl_->reverseMap.find(result.mBodyID);
    if (it != impl_->reverseMap.end()) {
      hit.entity = it->second;
    }
  }

  return hit;
}

uint32_t JoltPhysicsSystem::createStaticBody(const glm::vec3 &pos,
                                               const glm::quat &rot) {
  if (!impl_) {
    return kNoBody;
  }

  JPH::BodyCreationSettings settings;
  settings.mPosition = toJolt(pos);
  settings.mRotation = toJolt(rot);
  settings.mMotionType = JPH::EMotionType::Static;
  settings.mObjectLayer = Impl::kStaticLayer;
  settings.SetShape(new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F)));

  JPH::Body *body = impl_->bodyInterface->CreateBody(settings);
  if (body == nullptr) {
    return kNoBody;
  }

  impl_->bodyInterface->AddBody(body->GetID(),
                                JPH::EActivation::DontActivate);

  uint32_t id = impl_->nextBodyId++;
  impl_->bodyMap[id] = body->GetID();
  impl_->reverseMap[body->GetID()] = id;
  return id;
}

uint32_t JoltPhysicsSystem::createDynamicBody(const glm::vec3 &pos,
                                                const glm::quat &rot,
                                                float mass) {
  if (!impl_) {
    return kNoBody;
  }

  JPH::MassProperties massProps;
  massProps.mMass = mass;

  JPH::BodyCreationSettings settings;
  settings.mPosition = toJolt(pos);
  settings.mRotation = toJolt(rot);
  settings.mMotionType = JPH::EMotionType::Dynamic;
  settings.mObjectLayer = Impl::kDynamicLayer;
  settings.mMassPropertiesOverride = massProps;
  settings.mOverrideMassProperties =
      JPH::EOverrideMassProperties::CalculateInertia;
  settings.SetShape(new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F)));

  JPH::Body *body = impl_->bodyInterface->CreateBody(settings);
  if (body == nullptr) {
    return kNoBody;
  }

  impl_->bodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);

  uint32_t id = impl_->nextBodyId++;
  impl_->bodyMap[id] = body->GetID();
  impl_->reverseMap[body->GetID()] = id;
  return id;
}

uint32_t JoltPhysicsSystem::createKinematicBody(const glm::vec3 &pos,
                                                  const glm::quat &rot) {
  if (!impl_) {
    return kNoBody;
  }

  JPH::BodyCreationSettings settings;
  settings.mPosition = toJolt(pos);
  settings.mRotation = toJolt(rot);
  settings.mMotionType = JPH::EMotionType::Kinematic;
  settings.mObjectLayer = Impl::kDynamicLayer;
  settings.SetShape(new JPH::BoxShape(JPH::Vec3(0.5F, 0.5F, 0.5F)));

  JPH::Body *body = impl_->bodyInterface->CreateBody(settings);
  if (body == nullptr) {
    return kNoBody;
  }

  impl_->bodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);

  uint32_t id = impl_->nextBodyId++;
  impl_->bodyMap[id] = body->GetID();
  impl_->reverseMap[body->GetID()] = id;
  return id;
}

void JoltPhysicsSystem::destroyBody(uint32_t bodyId) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  impl_->bodyInterface->RemoveBody(it->second);
  impl_->bodyInterface->DestroyBody(it->second);
  impl_->reverseMap.erase(it->second);
  impl_->bodyMap.erase(it);
}

void JoltPhysicsSystem::setBodyTransform(uint32_t bodyId,
                                          const glm::vec3 &pos,
                                          const glm::quat &rot) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  impl_->bodyInterface->MoveKinematic(it->second, toJolt(pos), toJolt(rot),
                                       0.0F);
}

void JoltPhysicsSystem::getBodyTransform(uint32_t bodyId, glm::vec3 &pos,
                                          glm::quat &rot) const {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  pos = toGlm(impl_->bodyInterface->GetCenterOfMassPosition(it->second));
  rot = toGlm(impl_->bodyInterface->GetRotation(it->second));
}

void JoltPhysicsSystem::applyImpulse(uint32_t bodyId,
                                      const glm::vec3 &impulse) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  impl_->bodyInterface->AddImpulse(it->second, toJolt(impulse));
}

void JoltPhysicsSystem::addBoxCollider(uint32_t bodyId,
                                        const glm::vec3 &halfExtents) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  auto *shape = new JPH::BoxShape(toJolt(halfExtents));
  impl_->bodyInterface->SetShape(it->second, shape, true,
                                  JPH::EActivation::Activate);
}

void JoltPhysicsSystem::addSphereCollider(uint32_t bodyId, float radius) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  auto *shape = new JPH::SphereShape(radius);
  impl_->bodyInterface->SetShape(it->second, shape, true,
                                  JPH::EActivation::Activate);
}

void JoltPhysicsSystem::addCapsuleCollider(uint32_t bodyId, float radius,
                                             float height) {
  if (!impl_) {
    return;
  }

  auto it = impl_->bodyMap.find(bodyId);
  if (it == impl_->bodyMap.end()) {
    return;
  }

  auto *shape = new JPH::CapsuleShape(height * 0.5F, radius);
  impl_->bodyInterface->SetShape(it->second, shape, true,
                                  JPH::EActivation::Activate);
}

std::span<const ContactEvent> JoltPhysicsSystem::contacts() const {
  if (!impl_) {
    return {};
  }
  return impl_->contacts;
}

void JoltPhysicsSystem::clearContacts() {
  if (!impl_) {
    return;
  }
  impl_->contacts.clear();
}

} // namespace Raiden::Physics
