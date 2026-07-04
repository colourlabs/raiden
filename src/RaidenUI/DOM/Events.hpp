#pragma once

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenUI {

class EventSystem {
public:
  EventSystem() = default;

  void update(ElementNode *root, float mouseX, float mouseY);
  void click(ElementNode *root, float mouseX, float mouseY);

private:
  ElementNode *hitTest(ElementNode *node, float mx, float my) const;
  ElementNode *m_prevHover{nullptr};
};

} // namespace RaidenUI
