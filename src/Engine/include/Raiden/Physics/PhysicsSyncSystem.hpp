#pragma once

#include <Raiden/ECS/Entity.hpp>
#include <Raiden/ECS/System.hpp>

#include <unordered_map>
#include <cstdint>

namespace Raiden::Physics {

class IPhysicsSystem;

class PhysicsSyncSystem : public ECS::System {
public:
  explicit PhysicsSyncSystem(IPhysicsSystem &physics);

  void update(ECS::World &world, float dt) override;

  [[nodiscard]] ECS::Entity resolveEntity(uint32_t bodyId) const;

private:
  IPhysicsSystem &physics_;
  std::unordered_map<uint32_t, ECS::Entity> bodyToEntity_;
};

} // namespace Raiden::Physics
