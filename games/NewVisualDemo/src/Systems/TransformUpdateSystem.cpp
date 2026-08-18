#include "TransformUpdateSystem.hpp"

#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>

void TransformUpdateSystem::update(Raiden::ECS::World &world, float /*dt*/) {
  Raiden::ECS::updateTransforms(world);
}
