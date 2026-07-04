#include <RaidenEditor/Panels/InspectorPanel.hpp>

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenEditor {

InspectorPanel::InspectorPanel() = default;
InspectorPanel::~InspectorPanel() = default;

std::unique_ptr<RaidenUI::ElementNode> InspectorPanel::build() {
  auto panel = std::make_unique<RaidenUI::ElementNode>("div");
  panel->style["display"] = "flex";
  panel->style["flex-direction"] = "column";
  panel->style["flex"] = "1";
  panel->content = "Inspector";

  return panel;
}

void InspectorPanel::update(Raiden::ECS::World *world) {
  (void)world;
}

} // namespace RaidenEditor
