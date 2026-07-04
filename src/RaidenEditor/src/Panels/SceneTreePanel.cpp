#include <RaidenEditor/Panels/SceneTreePanel.hpp>

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenEditor {

std::unique_ptr<RaidenUI::ElementNode> SceneTreePanel::build() {
  auto panel = std::make_unique<RaidenUI::ElementNode>("div");

  panel->style["display"] = "flex";
  panel->style["flex-direction"] = "column";
  panel->style["min-width"] = "200px";
  panel->content = "Scene Tree";

  return panel;
}

void SceneTreePanel::refresh(Raiden::ECS::World *world) {
  (void)world;
}

} // namespace RaidenEditor
