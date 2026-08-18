#pragma once

#include <Raiden/ECS/Entity.hpp>
#include <Raiden/ECS/System.hpp>

#include <glm/vec3.hpp>

namespace Raiden::Audio { class IAudioDevice; }
namespace Raiden::Input { class ActionMap; }
namespace Raiden::Platform {
class IPlatform;
struct InputState;
}

class CameraControllerSystem final : public Raiden::ECS::System {
public:
  CameraControllerSystem(Raiden::Input::ActionMap &actions,
                         Raiden::Platform::IPlatform *platform,
                         Raiden::Audio::IAudioDevice *audio,
                         Raiden::ECS::Entity cameraEntity, glm::vec3 &position,
                         float &yaw, float &pitch, bool &mouseCaptured);

  void setInput(const Raiden::Platform::InputState &input) { input_ = &input; }
  void update(Raiden::ECS::World &world, float dt) override;

private:
  Raiden::Input::ActionMap &actions_;
  Raiden::Platform::IPlatform *platform_;
  Raiden::Audio::IAudioDevice *audio_;
  Raiden::ECS::Entity cameraEntity_;
  glm::vec3 &position_;
  float &yaw_;
  float &pitch_;
  bool &mouseCaptured_;
  const Raiden::Platform::InputState *input_ = nullptr;
  bool previousRightMouse_ = false;
};
