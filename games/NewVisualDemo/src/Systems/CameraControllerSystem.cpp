#include "CameraControllerSystem.hpp"

#include <Raiden/Audio/IAudioDevice.hpp>
#include <Raiden/Core/ConVar.hpp>
#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Input/ActionMap.hpp>
#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Platform/InputState.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

CameraControllerSystem::CameraControllerSystem(
    Raiden::Input::ActionMap &actions, Raiden::Platform::IPlatform *platform,
    Raiden::Audio::IAudioDevice *audio, Raiden::ECS::Entity cameraEntity,
    glm::vec3 &position, float &yaw, float &pitch, bool &mouseCaptured)
    : actions_(actions), platform_(platform), audio_(audio),
      cameraEntity_(cameraEntity), position_(position), yaw_(yaw),
      pitch_(pitch), mouseCaptured_(mouseCaptured) {}

void CameraControllerSystem::update(Raiden::ECS::World &world, float dt) {
  if (input_ == nullptr) return;

  if (input_->mouseButtons[2] && !previousRightMouse_) {
    mouseCaptured_ = !mouseCaptured_;
    if (platform_ != nullptr) platform_->setRelativeMouseMode(mouseCaptured_);
  }
  previousRightMouse_ = input_->mouseButtons[2];

  if (mouseCaptured_) {
    const float sensitivity =
        Raiden::Core::convars().getFloat("mouse_sensitivity", 0.002F);
    yaw_ += static_cast<float>(input_->mouseDeltaX) * sensitivity;
    pitch_ -= static_cast<float>(input_->mouseDeltaY) * sensitivity;
    pitch_ = glm::clamp(pitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  float speed = Raiden::Core::convars().getFloat("camera_speed", 6.0F) * dt;
  if (const auto *action = actions_.find("move_forward"); action && action->pressed)
    speed *= 2.0F;

  glm::vec3 forward(std::cos(yaw_) * std::cos(pitch_), std::sin(pitch_),
                    std::sin(yaw_) * std::cos(pitch_));
  forward = glm::normalize(forward);
  const glm::vec3 right = glm::normalize(glm::cross(forward, {0.0F, 1.0F, 0.0F}));
  auto pressed = [&](const char *name) {
    const auto *action = actions_.find(name);
    return action != nullptr && action->pressed;
  };
  if (pressed("move_forward")) position_ += forward * speed;
  if (pressed("move_back")) position_ -= forward * speed;
  if (pressed("move_left")) position_ -= right * speed;
  if (pressed("move_right")) position_ += right * speed;
  if (pressed("move_up")) position_.y += speed;
  if (pressed("move_down")) position_.y -= speed;

  auto &camera = world.get<Raiden::ECS::Camera>(cameraEntity_);
  camera.view = glm::lookAt(position_, position_ + forward, {0.0F, 1.0F, 0.0F});
  camera.setPerspective(70.0F, 16.0F / 9.0F, 0.1F, 300.0F);
  if (audio_ != nullptr) {
    audio_->setListenerPosition(position_.x, position_.y, position_.z);
    audio_->setListenerOrientation(forward.x, forward.y, forward.z, 0.0F, 1.0F, 0.0F);
  }
}
