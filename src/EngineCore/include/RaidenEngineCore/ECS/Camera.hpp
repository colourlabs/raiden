#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Raiden::Core {

struct Camera {
  glm::mat4 view = glm::mat4(1.0f);
  glm::mat4 projection = glm::mat4(1.0f);
  bool active = true; // engine picks the first active camera

  void setLookAt(glm::vec3 eye, glm::vec3 target,
                 glm::vec3 up = {0.0f, 1.0f, 0.0f}) {
    view = glm::lookAt(eye, target, up);
  }

  void setPerspective(float fovDeg, float aspect, float zNear, float zFar) {
    projection = glm::perspective(glm::radians(fovDeg), aspect, zNear, zFar);
    projection[1][1] *= -1.0f; // vulkan NDC has Y flipped vs OpenGL
  }

  void setOrthographic(float left, float right, float bottom, float top,
                       float zNear = -1.0f, float zFar = 1.0f) {
    projection = glm::ortho(left, right, bottom, top, zNear, zFar);
  }
};

} // namespace Raiden::Core