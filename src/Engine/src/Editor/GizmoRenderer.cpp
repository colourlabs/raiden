#include <Raiden/Editor/GizmoRenderer.hpp>
#include <Raiden/Logger.hpp>

#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

static const Raiden::Core::Logger s_logger("Raiden::Editor::GizmoRenderer");

static constexpr float kArrowLength = 1.0F;
static constexpr float kArrowHeadLen = 0.2F;
static constexpr float kArrowHeadRadius = 0.06F;
static constexpr float kCircleRadius = 0.8F;
static constexpr int kCircleSegments = 64;
static constexpr float kPlaneSize = 0.2F;
static constexpr float kCubeHalf = 0.05F;
static constexpr float kLinePickRadius = 0.08F;
static constexpr float kCirclePickThickness = 0.06F;

namespace Raiden::Editor {

using namespace ::Raiden::Renderer;

static constexpr std::array<glm::vec3, 3> kAxisColors = {{
    {1.0F, 0.2F, 0.2F}, // X = red
    {0.2F, 1.0F, 0.2F}, // Y = green
    {0.2F, 0.4F, 1.0F}, // Z = blue
}};

static constexpr glm::vec4 kHoverBright = {1.0F, 1.0F, 0.5F, 1.0F};

GizmoRenderer::~GizmoRenderer() { shutdown(); }

bool GizmoRenderer::init(IRenderDevice &device) {
  device_ = &device;

  linePipeline_ = device.createPipeline({
      .shader = {"shaders/gizmo.slang"},
      .vertexLayout =
          {
              .stride = sizeof(LineVertex),
              .attributes =
                  {
                      {.location = 0,
                       .format = Format::R32G32B32_Float,
                       .offset = offsetof(LineVertex, pos)},
                      {.location = 1,
                       .format = Format::R32G32B32A32_Float,
                       .offset = offsetof(LineVertex, color)},
                  },
          },
      .topology = Topology::LineList,
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .blendEnable = true,
      .cullMode = CullMode::None,
  });

  triPipeline_ = device.createPipeline({
      .shader = {"shaders/gizmo.slang"},
      .vertexLayout =
          {
              .stride = sizeof(LineVertex),
              .attributes =
                  {
                      {.location = 0,
                       .format = Format::R32G32B32_Float,
                       .offset = offsetof(LineVertex, pos)},
                      {.location = 1,
                       .format = Format::R32G32B32A32_Float,
                       .offset = offsetof(LineVertex, color)},
                  },
          },
      .topology = Topology::TriangleList,
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .blendEnable = true,
      .cullMode = CullMode::None,
  });

  lineBuffer_ = device.createBuffer({
      .size = sizeof(LineVertex) * 1024,
      .usage = BufferUsage::Vertex,
      .access = MemoryAccess::CpuToGpu,
  });

  triBuffer_ = device.createBuffer({
      .size = sizeof(LineVertex) * 2048,
      .usage = BufferUsage::Vertex,
      .access = MemoryAccess::CpuToGpu,
  });

  if (!linePipeline_ || !triPipeline_ || !lineBuffer_ || !triBuffer_) {
    s_logger.error("Failed to create gizmo resources");
    return false;
  }

  s_logger.info("GizmoRenderer initialized");
  return true;
}

void GizmoRenderer::shutdown() {
  linePipeline_.reset();
  triPipeline_.reset();
  lineBuffer_.reset();
  triBuffer_.reset();
  device_ = nullptr;
}

// geometry builders

GizmoRenderer::GizmoVertices GizmoRenderer::buildTranslateGizmo() {
  GizmoVertices gv;

  // axis lines
  for (int i = 0; i < 3; ++i) {
    glm::vec3 a(0.0F);
    glm::vec3 b = axisDir(i) * kArrowLength;
    glm::vec4 c = glm::vec4(kAxisColors[i], 1.0F);
    gv.lineVerts.push_back({a, c});
    gv.lineVerts.push_back({b, c});

    // arrow head (cone made of triangles)
    glm::vec3 tip = b;
    glm::vec3 baseCenter = b - axisDir(i) * kArrowHeadLen;
    glm::vec3 perpA;
    glm::vec3 perpB;
    if (i == 0) {
      perpA = {0, 1, 0};
      perpB = {0, 0, 1};
    } else if (i == 1) {
      perpA = {1, 0, 0};
      perpB = {0, 0, 1};
    } else {
      perpA = {1, 0, 0};
      perpB = {0, 1, 0};
    }

    constexpr int kConeSeg = 12;
    for (int s = 0; s < kConeSeg; ++s) {
      float a0 =
          static_cast<float>(s) / static_cast<float>(kConeSeg) * 6.2831853F;
      float a1 =
          static_cast<float>(s + 1) / static_cast<float>(kConeSeg) * 6.2831853F;
      glm::vec3 p0 =
          baseCenter +
          (std::cos(a0) * perpA + std::sin(a0) * perpB) * kArrowHeadRadius;
      glm::vec3 p1 =
          baseCenter +
          (std::cos(a1) * perpA + std::sin(a1) * perpB) * kArrowHeadRadius;
      gv.triVerts.push_back({p0, c});
      gv.triVerts.push_back({p1, c});
      gv.triVerts.push_back({tip, c});
    }
  }

  // plane quads (YZ, XZ, XY)
  static constexpr std::array<glm::vec4, 3> kPlaneColors = {{
      {1.0F, 0.2F, 0.2F, 0.25F},
      {0.2F, 1.0F, 0.2F, 0.25F},
      {0.2F, 0.4F, 1.0F, 0.25F},
  }};

  static constexpr std::array<glm::ivec2, 3> kPlaneAxes = {
      {{1, 2}, {0, 2}, {0, 1}}};

  for (int p = 0; p < 3; ++p) {
    int u = kPlaneAxes[p].x;
    int v = kPlaneAxes[p].y;
    glm::vec3 corner0(0.0F);
    glm::vec3 corner1 = axisDir(u) * kPlaneSize;
    glm::vec3 corner2 = axisDir(v) * kPlaneSize;
    glm::vec3 corner3 = corner1 + corner2;
    gv.triVerts.push_back({corner0, kPlaneColors[p]});
    gv.triVerts.push_back({corner1, kPlaneColors[p]});
    gv.triVerts.push_back({corner3, kPlaneColors[p]});
    gv.triVerts.push_back({corner0, kPlaneColors[p]});
    gv.triVerts.push_back({corner3, kPlaneColors[p]});
    gv.triVerts.push_back({corner2, kPlaneColors[p]});
  }

  return gv;
}

GizmoRenderer::GizmoVertices GizmoRenderer::buildRotateGizmo() {
  GizmoVertices gv;

  // 3 circles: YZ plane (X axis), XZ plane (Y axis), XY plane (Z axis)
  static constexpr std::array<glm::vec3, 3> kNormals = {
      {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
  for (int i = 0; i < 3; ++i) {
    glm::vec3 u;
    glm::vec3 v;
    if (i == 0) {
      u = {0, 1, 0};
      v = {0, 0, 1};
    } else if (i == 1) {
      u = {1, 0, 0};
      v = {0, 0, 1};
    } else {
      u = {1, 0, 0};
      v = {0, 1, 0};
    }

    glm::vec4 c = glm::vec4(kAxisColors[i], 1.0F);
    for (int s = 0; s < kCircleSegments; ++s) {
      float a0 = static_cast<float>(s) / static_cast<float>(kCircleSegments) *
                 6.2831853F;
      float a1 = static_cast<float>(s + 1) /
                 static_cast<float>(kCircleSegments) * 6.2831853F;
      glm::vec3 p0 =
          std::cos(a0) * u * kCircleRadius + std::sin(a0) * v * kCircleRadius;
      glm::vec3 p1 =
          std::cos(a1) * u * kCircleRadius + std::sin(a1) * v * kCircleRadius;
      gv.lineVerts.push_back({p0, c});
      gv.lineVerts.push_back({p1, c});
    }
  }

  // kNormals is indexed only implicitly via i above; kept for documentation of
  // axis order.
  (void)kNormals;

  return gv;
}

GizmoRenderer::GizmoVertices GizmoRenderer::buildScaleGizmo() {
  GizmoVertices gv;

  for (int i = 0; i < 3; ++i) {
    glm::vec3 a(0.0F);
    glm::vec3 b = axisDir(i) * kArrowLength;
    glm::vec4 c = glm::vec4(kAxisColors[i], 1.0F);
    gv.lineVerts.push_back({a, c});
    gv.lineVerts.push_back({b, c});

    // cube at tip
    glm::vec3 tip = b;
    float h = kCubeHalf;
    std::array<glm::vec3, 8> corners{};
    for (int j = 0; j < 8; ++j) {
      corners[j] = tip + glm::vec3((j & 1) != 0 ? h : -h, (j & 2) != 0 ? h : -h,
                                   (j & 4) != 0 ? h : -h);
    }
    // 12 edges of a cube
    static constexpr std::array<glm::ivec2, 12> kEdges = {{
        {0, 1},
        {2, 3},
        {4, 5},
        {6, 7},
        {0, 2},
        {1, 3},
        {4, 6},
        {5, 7},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},
    }};
    for (const auto &edge : kEdges) {
      gv.lineVerts.push_back({corners[edge.x], c});
      gv.lineVerts.push_back({corners[edge.y], c});
    }
  }

  return gv;
}

// ray utilities

GizmoRenderer::Ray GizmoRenderer::screenToRay(float mouseX, float mouseY,
                                              int vpW, int vpH,
                                              const glm::mat4 &view,
                                              const glm::mat4 &proj) {
  float x = (2.0F * mouseX / static_cast<float>(vpW)) - 1.0F;
  float y = 1.0F - (2.0F * mouseY / static_cast<float>(vpH));

  glm::mat4 invVP = glm::inverse(proj * view);
  glm::vec4 near4 = invVP * glm::vec4(x, y, -1.0F, 1.0F);
  glm::vec4 far4 = invVP * glm::vec4(x, y, 1.0F, 1.0F);
  near4 /= near4.w;
  far4 /= far4.w;

  return {.origin = glm::vec3(near4),
          .dir = glm::normalize(glm::vec3(far4) - glm::vec3(near4))};
}

float GizmoRenderer::rayLineSegment(const Ray &ray, const glm::vec3 &a,
                                    const glm::vec3 &b) {
  glm::vec3 ab = b - a;
  float abLen2 = glm::dot(ab, ab);
  if (abLen2 < 1e-8F) {
    return 1e30F;
  }

  float t = glm::dot(ray.origin - a, ab) / abLen2;
  t = glm::clamp(t, 0.0F, 1.0F);
  glm::vec3 closest = a + t * ab;
  float dist = glm::length(
      ray.origin + ray.dir * glm::dot(closest - ray.origin, ray.dir) - closest);

  // return parametric distance along ray for sorting
  float rayT = glm::dot(closest - ray.origin, ray.dir);
  return dist < kLinePickRadius ? rayT : 1e30F;
}

float GizmoRenderer::rayPlane(const Ray &ray, const glm::vec3 &planePoint,
                              const glm::vec3 &planeNormal) {
  float denom = glm::dot(ray.dir, planeNormal);
  if (std::abs(denom) < 1e-8F) {
    return 1e30F;
  }
  float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
  return t > 0.0F ? t : 1e30F;
}

float GizmoRenderer::rayCircle(const Ray &ray, const glm::vec3 &center,
                               const glm::vec3 &normal, float radius) {
  float t = rayPlane(ray, center, normal);
  if (t >= 1e30F) {
    return 1e30F;
  }

  glm::vec3 hit = ray.origin + ray.dir * t;
  float dist = glm::length(hit - center);
  return std::abs(dist - radius) < kCirclePickThickness ? t : 1e30F;
}

// axis helpers

glm::vec3 GizmoRenderer::axisDir(int axis) {
  switch (axis) {
  case 0:
    return {1, 0, 0};
  case 1:
    return {0, 1, 0};
  case 2:
    return {0, 0, 1};
  default:
    return {0, 0, 0};
  }
}

int GizmoRenderer::pickAxis(const Ray &ray, const glm::mat4 &gizmoWorld,
                            GizmoMode mode) {
  auto center = glm::vec3(gizmoWorld[3]);
  float bestDist = 1e30F;
  int bestAxis = -1;

  for (int i = 0; i < 3; ++i) {
    glm::vec3 a = center;
    glm::vec3 b = center + axisDir(i) * kArrowLength;
    float d = rayLineSegment(ray, a, b);
    if (d < bestDist) {
      bestDist = d;
      bestAxis = i;
    }
  }

  if (mode == GizmoMode::Rotate) {
    static constexpr std::array<glm::vec3, 3> kNormals = {
        {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    for (int i = 0; i < 3; ++i) {
      float d = rayCircle(ray, center, kNormals[i], kCircleRadius);
      if (d < bestDist) {
        bestDist = d;
        bestAxis = i;
      }
    }
  }

  return bestDist < 1e30F ? bestAxis : -1;
}

int GizmoRenderer::pickPlane(const Ray &ray, const glm::mat4 &gizmoWorld,
                             [[maybe_unused]] int axis) {
  auto center = glm::vec3(gizmoWorld[3]);
  float bestDist = 1e30F;
  int bestPlane = -1;

  static constexpr std::array<glm::ivec2, 3> kPlaneAxes = {
      {{1, 2}, {0, 2}, {0, 1}}};
  for (int p = 0; p < 3; ++p) {
    int u = kPlaneAxes[p].x;
    int v = kPlaneAxes[p].y;
    glm::vec3 normal = axisDir(3 - u - v); // the axis not in the plane
    float t = rayPlane(ray, center + axisDir(p) * kPlaneSize * 0.5F, normal);
    if (t < 1e30F) {
      glm::vec3 hit = ray.origin + ray.dir * t;
      float du = glm::abs(glm::dot(hit - center, axisDir(u)));
      float dv = glm::abs(glm::dot(hit - center, axisDir(v)));
      if (du < kPlaneSize && dv < kPlaneSize && t < bestDist) {
        bestDist = t;
        bestPlane = p;
      }
    }
  }

  return bestPlane;
}

// drag computation

void GizmoRenderer::applyTranslateDrag(
    const Ray &ray, int axis, int plane,
    [[maybe_unused]] const glm::mat4 &gizmoWorld,
    Raiden::ECS::Transform &transform) {
  glm::vec3 planeNormal = glm::normalize(glm::vec3(gizmoViewPlaneNormal_));
  float t = rayPlane(ray, dragStartPos_, planeNormal);
  if (t >= 1e30F) {
    return;
  }
  glm::vec3 hit = ray.origin + ray.dir * t;

  if (plane >= 0) {
    // plane drag: free movement on the constraint plane
    transform.translation = dragStartTransform_ + (hit - dragStartPos_);
  } else {
    // axis drag: project displacement onto the axis
    glm::vec3 axisDirVec = axisDir(axis);
    float proj = glm::dot(hit - dragStartPos_, axisDirVec);
    transform.translation = dragStartTransform_ + axisDirVec * proj;
  }
}

void GizmoRenderer::applyRotateDrag(const Ray &ray, int axis,
                                    const glm::mat4 &gizmoWorld,
                                    Raiden::ECS::Transform &transform) {
  auto center = glm::vec3(gizmoWorld[3]);
  glm::vec3 normal = axisDir(axis);

  float t = rayPlane(ray, center, normal);
  if (t >= 1e30F) {
    return;
  }

  glm::vec3 hit = ray.origin + ray.dir * t;
  glm::vec3 toHit = glm::normalize(hit - center);

  // build orthonormal basis in the rotation plane
  glm::vec3 u;
  glm::vec3 v;
  if (axis == 0) {
    u = {0, 1, 0};
    v = {0, 0, 1};
  } else if (axis == 1) {
    u = {1, 0, 0};
    v = {0, 0, 1};
  } else {
    u = {1, 0, 0};
    v = {0, 1, 0};
  }

  float angle = std::atan2(glm::dot(toHit, v), glm::dot(toHit, u));
  float delta = angle - dragStartAngle_;
  transform.rotation =
      glm::quat(dragStartRotation_) * glm::angleAxis(delta, normal);
}

void GizmoRenderer::applyScaleDrag(const Ray &ray, int axis,
                                   const glm::mat4 &gizmoWorld,
                                   Raiden::ECS::Transform &transform) {
  auto center = glm::vec3(gizmoWorld[3]);
  glm::vec3 axisDirVec = axisDir(axis);

  // find closest point on axis to mouse ray
  float t = rayPlane(
      ray, center,
      glm::normalize(glm::cross(axisDirVec, glm::normalize(ray.dir - center))));
  if (t >= 1e30F) {
    t = rayPlane(ray, center, axisDirVec);
  }
  if (t >= 1e30F) {
    return;
  }

  glm::vec3 hit = ray.origin + ray.dir * t;
  float currentDist = glm::dot(hit - center, axisDirVec);
  float startDist = glm::dot(dragStartPos_ - center, axisDirVec);

  if (std::abs(startDist) < 1e-6F) {
    return;
  }
  float factor = currentDist / startDist;
  float scaleValue = dragStartScale_ * factor;
  scaleValue = glm::max(scaleValue, 0.01F);

  if (axis == 0) {
    transform.scale.x = scaleValue;
  } else if (axis == 1) {
    transform.scale.y = scaleValue;
  } else {
    transform.scale.z = scaleValue;
  }
}

// render

void GizmoRenderer::render(ICommandBuffer &cmd, const glm::mat4 &viewProj,
                           const Raiden::ECS::Transform &transform,
                           GizmoMode mode) {
  GizmoVertices gv;
  switch (mode) {
  case GizmoMode::Translate:
    gv = buildTranslateGizmo();
    break;
  case GizmoMode::Rotate:
    gv = buildRotateGizmo();
    break;
  case GizmoMode::Scale:
    gv = buildScaleGizmo();
    break;
  case GizmoMode::None:
    break;
  }

  lineCount_ = static_cast<uint32_t>(gv.lineVerts.size());
  triCount_ = static_cast<uint32_t>(gv.triVerts.size());

  // highlight hovered axis
  if (hovered_ && activeAxis_ >= 0) {
    glm::vec4 bright = kHoverBright;
    for (auto &v : gv.lineVerts) {
      if (v.color == glm::vec4(kAxisColors[activeAxis_], 1.0F)) {
        v.color = bright;
      }
    }
  }

  glm::mat4 model = transform.worldMatrix;

  struct PushData {
    glm::mat4 viewProj;
    glm::mat4 model;
  };
  PushData pc{.viewProj = viewProj, .model = model};

  if (lineCount_ > 0) {
    lineBuffer_->upload(gv.lineVerts.data(), sizeof(LineVertex) * lineCount_);
    cmd.bindPipeline(*linePipeline_);
    cmd.bindVertexBuffer(*lineBuffer_);
    cmd.pushConstants(0, sizeof(pc), &pc);
    cmd.draw(lineCount_);
  }

  if (triCount_ > 0) {
    triBuffer_->upload(gv.triVerts.data(), sizeof(LineVertex) * triCount_);
    cmd.bindPipeline(*triPipeline_);
    cmd.bindVertexBuffer(*triBuffer_);
    cmd.pushConstants(0, sizeof(pc), &pc);
    cmd.draw(triCount_);
  }
}

// update (hit-test + drag)

bool GizmoRenderer::update(float mouseX, float mouseY, int vpW, int vpH,
                           const glm::mat4 &view, const glm::mat4 &proj,
                           Raiden::ECS::Transform &transform, GizmoMode mode,
                           const Raiden::Platform::InputState &input) {
  glm::mat4 gizmoWorld = transform.worldMatrix;
  Ray ray = screenToRay(mouseX, mouseY, vpW, vpH, view, proj);

  if (dragging_) {
    if (!input.mouseButtons[0]) {
      dragging_ = false;
      activeAxis_ = -1;
      activePlane_ = -1;
      return true;
    }

    switch (mode) {
    case GizmoMode::Translate:
      applyTranslateDrag(ray, activeAxis_, activePlane_, gizmoWorld, transform);
      break;
    case GizmoMode::Rotate:
      applyRotateDrag(ray, activeAxis_, gizmoWorld, transform);
      break;
    case GizmoMode::Scale:
      applyScaleDrag(ray, activeAxis_, gizmoWorld, transform);
      break;
    case GizmoMode::None:
      break;
    }
    return true;
  }

  // hover detection
  int axis = pickAxis(ray, gizmoWorld, mode);
  hovered_ = axis >= 0;

  if (input.mouseButtons[0] && hovered_) {
    dragging_ = true;
    activeAxis_ = axis;
    dragStartTransform_ = transform.translation;

    if (mode == GizmoMode::Translate) {
      activePlane_ = pickPlane(ray, gizmoWorld, axis);

      glm::vec3 planeNormal;
      if (activePlane_ >= 0) {
        // plane drag: normal is the axis perpendicular to the drag plane
        static constexpr std::array<glm::ivec2, 3> kPlaneAxes = {
            {{1, 2}, {0, 2}, {0, 1}}};
        int u = kPlaneAxes[activePlane_].x;
        int v = kPlaneAxes[activePlane_].y;
        planeNormal = axisDir(3 - u - v);
      } else {
        // axis drag: constraint plane contains the axis and faces the camera
        glm::vec3 axisDirVec = axisDir(axis);
        glm::vec3 camFwd = -glm::vec3(view[2][0], view[2][1], view[2][2]);
        planeNormal = glm::cross(axisDirVec, camFwd);
        if (glm::length(planeNormal) < 0.001F) {
          planeNormal = axisDirVec;
        } else {
          planeNormal = glm::normalize(planeNormal);
        }
      }
      gizmoViewPlaneNormal_ = glm::vec4(planeNormal, 0.0F);

      // initial hit on constraint plane
      auto center = glm::vec3(gizmoWorld[3]);
      float t = rayPlane(ray, center, planeNormal);
      dragStartPos_ = (t < 1e30F) ? (ray.origin + ray.dir * t) : center;
    } else if (mode == GizmoMode::Rotate) {
      auto center = glm::vec3(gizmoWorld[3]);
      glm::vec3 normal = axisDir(axis);
      float t = rayPlane(ray, center, normal);
      if (t < 1e30F) {
        glm::vec3 hit = ray.origin + ray.dir * t;
        glm::vec3 toHit = hit - center;
        glm::vec3 u;
        glm::vec3 v;
        if (axis == 0) {
          u = {0, 1, 0};
          v = {0, 0, 1};
        } else if (axis == 1) {
          u = {1, 0, 0};
          v = {0, 0, 1};
        } else {
          u = {1, 0, 0};
          v = {0, 1, 0};
        }
        dragStartAngle_ = std::atan2(glm::dot(toHit, v), glm::dot(toHit, u));
      }
      dragStartRotation_ =
          glm::vec4(transform.rotation.x, transform.rotation.y,
                    transform.rotation.z, transform.rotation.w);
    } else if (mode == GizmoMode::Scale) {
      if (axis == 0) {
        dragStartScale_ = transform.scale.x;
      } else if (axis == 1) {
        dragStartScale_ = transform.scale.y;
      } else {
        dragStartScale_ = transform.scale.z;
      }
    }

    return true;
  }

  return hovered_;
}

} // namespace Raiden::Editor