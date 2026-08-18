#pragma once

#include <Raiden/ECS/World.hpp>

namespace Raiden::Engine {

class EntityInspector {
public:
  EntityInspector() = default;

  void render(bool &open);
  void setWorld(::Raiden::ECS::World *w) { world_ = w; }

private:
  ::Raiden::ECS::World *world_ = nullptr;
  uint32_t selectedEntity_ = UINT32_MAX;
};

} // namespace Raiden::Engine
