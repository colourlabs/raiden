#pragma once

#include <memory>

namespace RaidenUI {
struct ElementNode;
} // namespace RaidenUI

namespace Raiden::ECS {
class World;
}

namespace RaidenEditor {

class SceneTreePanel {
public:
  SceneTreePanel();
  ~SceneTreePanel();

  SceneTreePanel(const SceneTreePanel &) = delete;
  SceneTreePanel &operator=(const SceneTreePanel &) = delete;

  SceneTreePanel(SceneTreePanel &&) noexcept = default;
  SceneTreePanel &operator=(SceneTreePanel &&) noexcept = default;

  static std::unique_ptr<RaidenUI::ElementNode> build();
  static void refresh(Raiden::ECS::World *world);

private:
  uint64_t selectedEntity_ = 0;
};

} // namespace RaidenEditor
