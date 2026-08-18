#include <Raiden/ECS/Camera.hpp>
#include <Raiden/ECS/Collider.hpp>
#include <Raiden/ECS/Light.hpp>
#include <Raiden/ECS/MeshRenderer.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Rigidbody.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/Engine/EntityInspector.hpp>
#include <numbers>

#include <imgui.h>

namespace Raiden::Engine {

static const char *componentDisplayName(std::string_view name) {
  if (!name.empty()) {
    return name.data();
  }
  return "(unknown)";
}

void EntityInspector::render(bool &open) {
  if (!open || (world_ == nullptr)) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(480, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Entity Inspector##EntityInspector", &open)) {
    ImGui::End();
    return;
  }

  // split into left (list) and right (detail)
  ImGui::Columns(2, "InspectorSplit", true);
  ImGui::SetColumnWidth(0, 180.0F);

  // left panel: entity list
  ImGui::Text("Entities");
  ImGui::Separator();

  ImGui::BeginChild("EntityList", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);

  world_->view<::Raiden::ECS::Name>().each(
      [&](::Raiden::ECS::Entity e, ::Raiden::ECS::Name &name) {
        ImGui::PushID(static_cast<int>(e.index));

        bool selected = (selectedEntity_ == e.index);
        if (ImGui::Selectable(name.value.c_str(), selected)) {
          selectedEntity_ = e.index;
        }

        // right-click context menu
        if (ImGui::BeginPopupContextItem()) {
          if (ImGui::MenuItem("Select")) {
            selectedEntity_ = e.index;
          }
          ImGui::EndPopup();
        }

        ImGui::PopID();
      });

  // also list entities without Name (rare)
  world_->view<::Raiden::ECS::Transform>().each(
      [&](::Raiden::ECS::Entity e, ::Raiden::ECS::Transform &) {
        // only show if entity wasn't already shown (has no Name component)
        if (!world_->has<::Raiden::ECS::Name>(e)) {
          char label[32];
          snprintf(label, sizeof(label), "Entity[%u]", e.index);

          bool selected = (selectedEntity_ == e.index);
          if (ImGui::Selectable(label, selected)) {
            selectedEntity_ = e.index;
          }
        }
      });

  ImGui::EndChild();
  ImGui::NextColumn();

  // right panel: component details
  if (selectedEntity_ == UINT32_MAX) {
    ImGui::Text("Select an entity");
    ImGui::End();
    ImGui::Columns(1);
    return;
  }

  ::Raiden::ECS::Entity e{.index = selectedEntity_, .generation = 0};

  // verify entity is valid
  if (e.index >= world_->entityCount()) {
    ImGui::Text("Invalid entity");
    ImGui::End();
    ImGui::Columns(1);
    return;
  }

  // get entity name for header
  bool hasName = world_->has<::Raiden::ECS::Name>(e);
  if (hasName) {
    auto &name = world_->get<::Raiden::ECS::Name>(e);
    ImGui::Text("%s [%u]", name.value.c_str(), e.index);
  } else {
    ImGui::Text("Entity [%u]", e.index);
  }
  ImGui::Separator();

  ImGui::BeginChild("ComponentDetails", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);

  world_->forEachComponent(e, [&](::Raiden::ECS::Entity,
                                  ::Raiden::ECS::ComponentId,
                                  std::string_view compName, void *data) {
    if (ImGui::CollapsingHeader(componentDisplayName(compName),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent(12.0F);

      // Name
      if (compName == "Name") {
        auto *name = static_cast<::Raiden::ECS::Name *>(data);
        char buf[256] = {};
        strncpy(buf, name->value.c_str(), sizeof(buf) - 1);
        if (ImGui::InputText("Value", buf, sizeof(buf),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
          name->value = buf;
        }
      }
      // Transform
      else if (compName == "Transform") {
        auto *t = static_cast<::Raiden::ECS::Transform *>(data);
        ImGui::DragFloat3("Position", &t->translation.x, 0.1F);
        ImGui::DragFloat3("Scale", &t->scale.x, 0.05F, 0.01F, 100.0F);

        // show euler angles (read-only display)
        glm::vec3 euler = glm::eulerAngles(t->rotation) *
                          (180.0F / std::numbers::pi_v<float>);
        ImGui::Text("Rotation: (%.1f, %.1f, %.1f) deg", euler.x, euler.y,
                    euler.z);

        ImGui::Separator();
        ImGui::Text("World Matrix:");
        for (int row = 0; row < 4; ++row) {
          ImGui::Text("  [%.3f, %.3f, %.3f, %.3f]", t->worldMatrix[0][row],
                      t->worldMatrix[1][row], t->worldMatrix[2][row],
                      t->worldMatrix[3][row]);
        }
      }

      // MeshRenderer
      else if (compName == "MeshRenderer") {
        auto *mr = static_cast<::Raiden::ECS::MeshRenderer *>(data);
        ImGui::Text("Mesh: %s", mr->meshPath.c_str());
        ImGui::Text("Texture: %s", mr->texturePath.empty()
                                       ? "(none)"
                                       : mr->texturePath.c_str());
        ImGui::Text("Normal: %s",
                    mr->normalMap.empty() ? "(none)" : mr->normalMap.c_str());
        ImGui::Text("Shader: %s", mr->shader.c_str());
        ImGui::Separator();
        ImGui::ColorEdit4("Base Color", &mr->baseColorFactor.x);
        ImGui::SliderFloat("Metallic", &mr->metallic, 0.0F, 1.0F);
        ImGui::SliderFloat("Roughness", &mr->roughness, 0.0F, 1.0F);
        ImGui::Checkbox("Cast Shadows", &mr->castShadows);
      }
      // Camera
      else if (compName == "Camera") {
        auto *cam = static_cast<::Raiden::ECS::Camera *>(data);
        ImGui::Checkbox("Active", &cam->active);
        ImGui::SliderFloat("FOV", &cam->fov, 10.0F, 120.0F, "%.1f deg");
        ImGui::DragFloat("Near", &cam->zNear, 0.01F, 0.001F, 10.0F);
        ImGui::DragFloat("Far", &cam->zFar, 1.0F, 1.0F, 10000.0F);
      }
      // DirectionalLight
      else if (compName == "DirectionalLight") {
        auto *dl = static_cast<::Raiden::ECS::DirectionalLight *>(data);
        ImGui::DragFloat3("Direction", &dl->direction.x, 0.05F, -1.0F, 1.0F);
        ImGui::ColorEdit3("Color", &dl->color.x);
        ImGui::SliderFloat("Intensity", &dl->intensity, 0.0F, 10.0F);
        ImGui::Checkbox("Cast Shadows", &dl->castShadows);
        if (dl->castShadows) {
          ImGui::DragFloat("Shadow Near", &dl->shadowNear, 0.1F, 0.01F, 50.0F);
          ImGui::DragFloat("Shadow Far", &dl->shadowFar, 1.0F, 1.0F, 200.0F);
          ImGui::DragFloat("Shadow Size", &dl->shadowSize, 0.5F, 1.0F, 200.0F);
          ImGui::Text("Shadow Map: %u px", dl->shadowMapResolution);
        }
      }
      // PointLight
      else if (compName == "PointLight") {
        auto *pl = static_cast<::Raiden::ECS::PointLight *>(data);
        ImGui::DragFloat3("Position", &pl->position.x, 0.1F);
        ImGui::ColorEdit3("Color", &pl->color.x);
        ImGui::SliderFloat("Intensity", &pl->intensity, 0.0F, 50.0F);
        ImGui::DragFloat("Range", &pl->range, 1.0F, 0.1F, 100.0F);
      }
      // AmbientLight
      else if (compName == "AmbientLight") {
        auto *al = static_cast<::Raiden::ECS::AmbientLight *>(data);
        ImGui::ColorEdit3("Sky Color", &al->skyColor.x);
        ImGui::SliderFloat("Sky Intensity", &al->skyIntensity, 0.0F, 5.0F);
        ImGui::ColorEdit3("Ground Color", &al->groundColor.x);
        ImGui::Checkbox("Use IBL", &al->useIBL);
      }
      // Rigidbody
      else if (compName == "Rigidbody") {
        auto *rb = static_cast<::Raiden::ECS::Rigidbody *>(data);
        const char *types[] = {"Static", "Kinematic", "Dynamic"};
        int current = static_cast<int>(rb->type);
        if (ImGui::Combo("Type", &current, types, 3)) {
          rb->type = static_cast<::Raiden::ECS::Rigidbody::Type>(current);
        }
        ImGui::DragFloat("Mass", &rb->mass, 0.1F, 0.0F, 1000.0F);
        ImGui::SliderFloat("Friction", &rb->friction, 0.0F, 1.0F);
        ImGui::SliderFloat("Restitution", &rb->restitution, 0.0F, 1.0F);
      }
      // Collider
      else if (compName == "Collider") {
        auto *col = static_cast<::Raiden::ECS::Collider *>(data);
        const char *shapes[] = {"Box", "Sphere", "Capsule"};
        int current = static_cast<int>(col->shape);
        if (ImGui::Combo("Shape", &current, shapes, 3)) {
          col->shape = static_cast<::Raiden::ECS::Collider::Shape>(current);
        }
        if (col->shape == ::Raiden::ECS::Collider::Shape::Box) {
          ImGui::DragFloat3("Half Extents", &col->halfExtents.x, 0.05F);
        } else if (col->shape == ::Raiden::ECS::Collider::Shape::Sphere) {
          ImGui::DragFloat("Radius", &col->radius, 0.05F, 0.0F, 100.0F);
        } else if (col->shape == ::Raiden::ECS::Collider::Shape::Capsule) {
          ImGui::DragFloat("Radius", &col->radius, 0.05F, 0.0F, 100.0F);
          ImGui::DragFloat("Height", &col->height, 0.1F, 0.0F, 100.0F);
        }
      }
      // Unknown
      else {
        ImGui::TextDisabled("(no editor available)");
      }

      ImGui::Unindent(12.0F);
    }
  });

  ImGui::EndChild();
  ImGui::End();
  ImGui::Columns(1);
}

} // namespace Raiden::Engine
