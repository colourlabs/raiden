#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Raiden::ECS {

struct Camera {
  glm::mat4 view = glm::mat4(1.0F);
  glm::mat4 projection = glm::mat4(1.0F);
  bool active = true; // engine picks the first active camera

  float fov = 45.0F;
  float zNear = 0.1F;
  float zFar = 100.0F;

  void setLookAt(glm::vec3 eye, glm::vec3 target,
                 glm::vec3 up = {0.0F, 1.0F, 0.0F}) {
    view = glm::lookAt(eye, target, up);
  }

  void setPerspective(float fovDeg, float aspect, float nearPlane, float farPlane) {
    fov = fovDeg;
    zNear = nearPlane;
    zFar = farPlane;
    projection = glm::perspective(glm::radians(fov), aspect, zNear, zFar);
    projection[1][1] *= -1.0F;
  }

  void setOrthographic(float left, float right, float bottom, float top,
                       float nearZ = -1.0F, float farZ = 1.0F) {
    projection = glm::ortho(left, right, bottom, top, nearZ, farZ);
  }
};

} // namespace Raiden::ECS