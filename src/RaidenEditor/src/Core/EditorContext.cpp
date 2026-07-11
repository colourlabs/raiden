#include <RaidenEditor/Core/EditorContext.hpp>

#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/Logger.hpp>

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

static const Raiden::Core::Logger s_logger("EditorContext");

namespace RaidenEditor {

EditorContext::EditorContext(QObject *parent) : QObject(parent) {}

void EditorContext::setWorld(Raiden::ECS::World *world) {
  if (world_ == world) {
    return;
  }

  destroyEditorCamera();
  world_ = world;
  if (world_ != nullptr) {
    initEditorCamera();
  }
  emit worldChanged(world_);
}

void EditorContext::setSelection(Raiden::ECS::Entity entity) {
  if (selection_ == entity) {
    return;
  }

  selection_ = entity;
  emit selectionChanged(selection_);
}

void EditorContext::setCurrentScenePath(const std::string &path) {
  if (currentScenePath_ == path) {
    return;
  }
  currentScenePath_ = path;
  dirty_ = false;
  emit scenePathChanged(currentScenePath_);
}

void EditorContext::markDirty() {
  if (!dirty_) {
    dirty_ = true;
    emit sceneModified();
  }
}

void EditorContext::clearDirty() { dirty_ = false; }

bool EditorContext::initGizmoRenderer(::Raiden::Renderer::IRenderDevice &device) {
  gizmoRenderer_ = std::make_unique<Raiden::Editor::GizmoRenderer>();
  if (!gizmoRenderer_->init(device)) {
    s_logger.error("Failed to initialize gizmo renderer");
    gizmoRenderer_.reset();
    return false;
  }
  s_logger.info("Gizmo renderer initialized");
  return true;
}

void EditorContext::shutdownGizmoRenderer() {
  if (gizmoRenderer_) {
    gizmoRenderer_->shutdown();
    gizmoRenderer_.reset();
  }
}

bool EditorContext::updateGizmo(const Raiden::Platform::InputState &input,
                               int vpW, int vpH,
                               const glm::mat4 &view, const glm::mat4 &proj) {
  if (!gizmoRenderer_ || world_ == nullptr || selection_.index == 0 ||
      gizmoMode_ == Raiden::Editor::GizmoMode::None) {
    return false;
  }

  if (selection_ == editorCamera_) {
    return false;
  }

  auto &t = world_->get<Raiden::ECS::Transform>(selection_);
  return gizmoRenderer_->update(
      static_cast<float>(input.mouseX), static_cast<float>(input.mouseY),
      vpW, vpH, view, proj, t, gizmoMode_, input);
}

void EditorContext::initEditorCamera() {
  if (world_ == nullptr || editorCamera_.index != 0) {
    return;
  }

  editorCamera_ = world_->create();
  world_->assign<Raiden::ECS::Name>(editorCamera_, "Editor Camera");
  world_->assign<Raiden::ECS::Camera>(editorCamera_);
  world_->assign<Raiden::ECS::Transform>(editorCamera_);

  auto &cam = world_->get<Raiden::ECS::Camera>(editorCamera_);
  cam.active = editorMode_;
  cam.setPerspective(45.0F, 1.0F, 0.1F, 1000.0F);
  cam.view = glm::lookAt(camPos_, camPos_ + glm::vec3(0.0F, 0.0F, -1.0F),
                         glm::vec3(0.0F, 1.0F, 0.0F));

  s_logger.info("Editor camera created.");
}

void EditorContext::destroyEditorCamera() {
  if (world_ == nullptr || editorCamera_.index == 0) {
    return;
  }

  world_->destroy(editorCamera_);
  editorCamera_ = {};
  s_logger.info("Editor camera destroyed.");
}

void EditorContext::setEditorMode(bool enabled) {
  if (editorMode_ == enabled) {
    return;
  }

  editorMode_ = enabled;

  if (world_ == nullptr || editorCamera_.index == 0) {
    return;
  }

  auto &cam = world_->get<Raiden::ECS::Camera>(editorCamera_);
  cam.active = editorMode_;

  world_->view<Raiden::ECS::Camera>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Camera &c) {
        if (e != editorCamera_) {
          c.active = !editorMode_;
        }
      });

  s_logger.info("Viewport mode: {}", editorMode_ ? "Scene" : "Game");
}

void EditorContext::updateEditorCamera(float deltaTime,
                                       const Raiden::Platform::InputState &input) {
  if (!editorMode_ || world_ == nullptr || editorCamera_.index == 0) {
    return;
  }

  static bool prevMmb = false;
  if (input.mouseButtons[1] && !prevMmb) {
    camCaptured_ = !camCaptured_;
  }
  prevMmb = input.mouseButtons[1];

  // block camera movement while gizmo is being manipulated
  if (gizmoRenderer_ && gizmoRenderer_->isDragging()) {
    camCaptured_ = false;
    return;
  }

  if (overlay_ != nullptr && overlay_->wantsCaptureMouse()) {
    camCaptured_ = false;
    return;
  }

  if (camCaptured_) {
    float sensitivity = 0.003F;
    camYaw_ += static_cast<float>(input.mouseDeltaX) * sensitivity;
    camPitch_ -= static_cast<float>(input.mouseDeltaY) * sensitivity;
    camPitch_ =
        glm::clamp(camPitch_, glm::radians(-89.0F), glm::radians(89.0F));
  }

  float speed = 5.0F * deltaTime;
  if (input.keysDown[42]) {
    speed *= 3.0F;
  }

  glm::vec3 forward(std::cos(camYaw_) * std::cos(camPitch_), std::sin(camPitch_),
                    std::sin(camYaw_) * std::cos(camPitch_));
  forward = glm::normalize(forward);

  glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0F, 1.0F, 0.0F)));
  glm::vec3 up = glm::cross(right, forward);

  if (input.keysDown[26]) {
    camPos_ += forward * speed;
  }
  if (input.keysDown[22]) {
    camPos_ -= forward * speed;
  }
  if (input.keysDown[4]) {
    camPos_ -= right * speed;
  }
  if (input.keysDown[7]) {
    camPos_ += right * speed;
  }
  if (input.keysDown[8]) {
    camPos_ += up * speed;
  }
  if (input.keysDown[20]) {
    camPos_ -= up * speed;
  }

  auto &cam = world_->get<Raiden::ECS::Camera>(editorCamera_);
  cam.view = glm::lookAt(camPos_, camPos_ + forward, glm::vec3(0.0F, 1.0F, 0.0F));
}

void EditorContext::setGizmoMatrices(const float *view, const float *proj) {
  std::memcpy(gizmoViewMatrix_, view, sizeof(gizmoViewMatrix_));
  std::memcpy(gizmoProjMatrix_, proj, sizeof(gizmoProjMatrix_));
}

void EditorContext::renderGizmoGeometry(::Raiden::Renderer::ICommandBuffer &cmd) {
  if (!gizmoRenderer_ || world_ == nullptr || selection_.index == 0 ||
      gizmoMode_ == Raiden::Editor::GizmoMode::None) {
    return;
  }

  if (selection_ == editorCamera_) {
    return;
  }

  // read camera matrices from overlay (set by Application each frame)
  const float *viewMat = nullptr;
  const float *projMat = nullptr;
  if (overlay_) {
    viewMat = overlay_->cameraViewMatrix();
    projMat = overlay_->cameraProjMatrix();
  } else {
    viewMat = gizmoViewMatrix_;
    projMat = gizmoProjMatrix_;
  }

  auto &t = world_->get<Raiden::ECS::Transform>(selection_);
  glm::mat4 view = glm::make_mat4(viewMat);
  glm::mat4 proj = glm::make_mat4(projMat);
  glm::mat4 viewProj = proj * view;

  gizmoRenderer_->render(cmd, viewProj, t, gizmoMode_);
}

} // namespace RaidenEditor
