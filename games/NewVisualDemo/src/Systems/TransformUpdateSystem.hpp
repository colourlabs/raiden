#pragma once

#include <Raiden/ECS/System.hpp>

class TransformUpdateSystem final : public Raiden::ECS::System {
public:
  void update(Raiden::ECS::World &world, float dt) override;
};
