#pragma once

#include <RaidenEditor/GizmoMode.hpp>
#include <Raiden/ECS/Entity.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Editor/GizmoRenderer.hpp>
#include <Raiden/Platform/InputState.hpp>

#include <QObject>
#include <QUndoStack>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <string>

namespace Raiden::Engine {
class ImGuiOverlay;
}

namespace RaidenEditor {

class EditorContext : public QObject {
  Q_OBJECT
public:
  explicit EditorContext(QObject *parent = nullptr);

  void setWorld(Raiden::ECS::World *world);
  [[nodiscard]] Raiden::ECS::World *world() const { return world_; }

  [[nodiscard]] Raiden::ECS::Entity selection() const { return selection_; }
  [[nodiscard]] QUndoStack *undoStack() { return &undoStack_; }

  void setSelection(Raiden::ECS::Entity entity);

  void setCurrentScenePath(const std::string &path);
  [[nodiscard]] const std::string &currentScenePath() const {
    return currentScenePath_;
  }

  void markDirty();
  [[nodiscard]] bool isDirty() const { return dirty_; }
  void clearDirty();

  // viewport mode: Scene (editor camera) or Game (game camera)
  void setEditorMode(bool enabled);
  [[nodiscard]] bool isEditorMode() const { return editorMode_; }

  // call each frame to update the editor camera
  void updateEditorCamera(float deltaTime, const Raiden::Platform::InputState &input);

  // create/destroy the editor camera in the world
  void initEditorCamera();
  void destroyEditorCamera();
  [[nodiscard]] Raiden::ECS::Entity editorCameraEntity() const { return editorCamera_; }

  // gizmo
  void setGizmoMode(Raiden::Editor::GizmoMode mode) { gizmoMode_ = mode; }
  [[nodiscard]] Raiden::Editor::GizmoMode gizmoMode() const { return gizmoMode_; }
  void setOverlay(Raiden::Engine::ImGuiOverlay *overlay) { overlay_ = overlay; }
  [[nodiscard]] Raiden::Engine::ImGuiOverlay *overlay() const { return overlay_; }
  bool initGizmoRenderer(::Raiden::Renderer::IRenderDevice &device);
  void shutdownGizmoRenderer();
  [[nodiscard]] Raiden::Editor::GizmoRenderer *gizmoRenderer() { return gizmoRenderer_.get(); }

  // update gizmo hit-testing and drag; returns true if gizmo consumed input
  bool updateGizmo(const Raiden::Platform::InputState &input,
                   int vpW, int vpH,
                   const glm::mat4 &view, const glm::mat4 &proj);

  void setGizmoMatrices(const float *view, const float *proj);

  // render gizmo geometry (called from Application's drawFrame callback)
  void renderGizmoGeometry(::Raiden::Renderer::ICommandBuffer &cmd);

signals:
  void selectionChanged(Raiden::ECS::Entity entity);
  void worldChanged(Raiden::ECS::World *world);
  void sceneModified();
  void scenePathChanged(const std::string &path);

private:
  Raiden::ECS::World *world_ = nullptr;
  Raiden::ECS::Entity selection_{};
  QUndoStack undoStack_;
  std::string currentScenePath_;
  bool dirty_ = false;

  // editor camera
  Raiden::ECS::Entity editorCamera_{};
  bool editorMode_ = true;
  glm::vec3 camPos_{0.0F, 0.0F, 3.0F};
  float camYaw_ = -1.5707963F;
  float camPitch_ = 0.0F;
  bool camCaptured_ = false;

  // gizmo
  Raiden::Engine::ImGuiOverlay *overlay_ = nullptr;
  std::unique_ptr<Raiden::Editor::GizmoRenderer> gizmoRenderer_;
  Raiden::Editor::GizmoMode gizmoMode_ = Raiden::Editor::GizmoMode::Translate;
  float gizmoViewMatrix_[16]{};
  float gizmoProjMatrix_[16]{};
};

} // namespace RaidenEditor

Q_DECLARE_METATYPE(Raiden::ECS::Entity)
