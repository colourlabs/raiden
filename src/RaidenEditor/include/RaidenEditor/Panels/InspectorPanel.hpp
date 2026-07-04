#pragma once

#include <memory>

namespace RaidenUI {
struct ElementNode;
} // namespace RaidenUI

namespace Raiden::ECS {
class World;
}

namespace RaidenEditor {

class InspectorPanel {
public:
  InspectorPanel();
  ~InspectorPanel();

  InspectorPanel(const InspectorPanel &) = delete;
  InspectorPanel &operator=(const InspectorPanel &) = delete;

  InspectorPanel(InspectorPanel &&) noexcept = default;
  InspectorPanel &operator=(InspectorPanel &&) noexcept = default;

  static std::unique_ptr<RaidenUI::ElementNode> build();
  static void update(Raiden::ECS::World *world);

private:
  // selected entity handle (stored as uint64_t for now)
  uint64_t selectedEntity_ = 0;
};

} // namespace RaidenEditor
