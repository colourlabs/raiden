#pragma once

namespace Raiden::ECS {

class World;

class System {
public:
  System() = default;
  System(const System&) = delete;
  System& operator=(const System&) = delete;
  System(System&&) = delete;
  System& operator=(System&&) = delete;
  
  virtual ~System() = default;

  virtual void update(World &world, float dt) = 0;
  virtual void render(World &world) { (void)world; }
};

} // namespace Raiden::ECS
