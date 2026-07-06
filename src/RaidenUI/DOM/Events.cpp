#include <RaidenUI/DOM/Events.hpp>
#include <RaidenUI/CSS/Selector.hpp>

#include <ranges>

namespace RaidenUI {

ElementNode *EventSystem::hitTest(ElementNode *node, float mx,
                                  float my) const {
  if (!node->visible) { return nullptr; }

  // children are painted in order, last child is topmost
  // walk in reverse so we hit the topmost element first
  for (auto &child : node->children | std::views::reverse) {
    auto *found = hitTest(child.get(), mx, my);
    if (found != nullptr) { return found; }
  }

  // check self
  if (mx >= node->computedX && mx < node->computedX + node->computedWidth &&
      my >= node->computedY && my < node->computedY + node->computedHeight) {
    return node;
  }

  return nullptr;
}

void EventSystem::update(ElementNode *root, float mouseX, float mouseY) {
  auto *target = hitTest(root, mouseX, mouseY);

  if (target != m_prevHover) {
    if (m_prevHover != nullptr) {
      m_prevHover->hovered = false;
      markStyleDirty(m_prevHover);
      if (m_prevHover->onMouseLeave) { m_prevHover->onMouseLeave(); }
    }

    if (target != nullptr) {
      target->hovered = true;
      markStyleDirty(target);
      if (target->onMouseEnter) { target->onMouseEnter(); }
    }

    m_prevHover = target;
  }
}

void EventSystem::click(ElementNode *root, float mouseX, float mouseY) {
  auto *target = hitTest(root, mouseX, mouseY);
  if (target != nullptr && target->onClick) {
    target->onClick();
  }
}

} // namespace RaidenUI
