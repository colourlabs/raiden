#pragma once

#include <Raiden/ECS/Transform.hpp>
#include <Raiden/Editor/GizmoMode.hpp>
#include <Raiden/Platform/InputState.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>
#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Raiden::Editor {

class GizmoRenderer {
public:
  GizmoRenderer() = default;
  ~GizmoRenderer();

  GizmoRenderer(const GizmoRenderer &) = delete;
  GizmoRenderer &operator=(const GizmoRenderer &) = delete;
  GizmoRenderer(GizmoRenderer &&) = delete;
  GizmoRenderer &operator=(GizmoRenderer &&) = delete;

  bool init(::Raiden::Renderer::IRenderDevice &device);
  void shutdown();

  // render the gizmo at the given transform
  void render(::Raiden::Renderer::ICommandBuffer &cmd,
              const glm::mat4 &viewProj,
              const ::Raiden::ECS::Transform &transform,
              ::Raiden::Editor::GizmoMode mode);

  // update hit-testing and drag logic; returns true if gizmo consumed input
  bool update(float mouseX, float mouseY, int vpW, int vpH,
              const glm::mat4 &view, const glm::mat4 &proj,
              ::Raiden::ECS::Transform &transform,
              ::Raiden::Editor::GizmoMode mode,
              const ::Raiden::Platform::InputState &input);

  [[nodiscard]] bool isHovered() const { return hovered_; }
  [[nodiscard]] bool isDragging() const { return dragging_; }

private:
  // geometry builders
  struct GizmoVertices {
    std::vector<::Raiden::Renderer::LineVertex> lineVerts;
    std::vector<::Raiden::Renderer::LineVertex> triVerts;
  };

  [[nodiscard]] static GizmoVertices buildTranslateGizmo();
  [[nodiscard]] static GizmoVertices buildRotateGizmo();
  [[nodiscard]] static GizmoVertices buildScaleGizmo();

  // ray utilities
  struct Ray {
    glm::vec3 origin;
    glm::vec3 dir;
  };

  [[nodiscard]] static Ray screenToRay(float mouseX, float mouseY, int vpW,
                                       int vpH, const glm::mat4 &view,
                                       const glm::mat4 &proj);

  // hit-test returns distance to ray (inf = miss)
  [[nodiscard]] static float rayLineSegment(const Ray &ray, const glm::vec3 &a,
                                            const glm::vec3 &b);
  [[nodiscard]] static float rayPlane(const Ray &ray,
                                      const glm::vec3 &planePoint,
                                      const glm::vec3 &planeNormal);
  [[nodiscard]] static float rayCircle(const Ray &ray, const glm::vec3 &center,
                                       const glm::vec3 &normal, float radius);

  // axis helpers
  [[nodiscard]] static glm::vec3 axisDir(int axis);
  [[nodiscard]] static int pickAxis(const Ray &ray, const glm::mat4 &gizmoWorld,
                                    ::Raiden::Editor::GizmoMode mode);
  [[nodiscard]] static int pickPlane(const Ray &ray,
                                     const glm::mat4 &gizmoWorld, int axis);

  // drag computation
  void applyTranslateDrag(const Ray &ray, int axis, int plane,
                          const glm::mat4 &gizmoWorld,
                          ::Raiden::ECS::Transform &transform);
  void applyRotateDrag(const Ray &ray, int axis, const glm::mat4 &gizmoWorld,
                       ::Raiden::ECS::Transform &transform);
  void applyScaleDrag(const Ray &ray, int axis, const glm::mat4 &gizmoWorld,
                      ::Raiden::ECS::Transform &transform);

  ::Raiden::Renderer::IRenderDevice *device_ = nullptr;
  std::unique_ptr<::Raiden::Renderer::IPipeline> linePipeline_;
  std::unique_ptr<::Raiden::Renderer::IPipeline> triPipeline_;
  std::unique_ptr<::Raiden::Renderer::IBuffer> lineBuffer_;
  std::unique_ptr<::Raiden::Renderer::IBuffer> triBuffer_;
  uint32_t lineCount_ = 0;
  uint32_t triCount_ = 0;

  // hit-testing state
  bool hovered_ = false;
  bool dragging_ = false;
  int activeAxis_ = -1;  // 0=X, 1=Y, 2=Z
  int activePlane_ = -1; // -1=line drag, 0=YZ, 1=XZ, 2=XY
  glm::vec3 dragStartPos_{0.0F};
  glm::vec3 dragStartTransform_{0.0F};
  float dragStartAngle_ = 0.0F;
  float dragStartScale_ = 0.0F;
  float dragAccum_ = 0.0F;
  glm::vec4 dragStartRotation_{0.0F, 0.0F, 0.0F, 1.0F};
  glm::vec4 gizmoViewPlaneNormal_{0.0F, 0.0F, -1.0F, 0.0F};

  // gizmo scale (screen-space size)
  float gizmoScale_ = 80.0F;
};

} // namespace Raiden::Editor
