#pragma once

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenUI {

class EventSystem {
public:
  EventSystem() = default;

  void update(ElementNode *root, float mouseX, float mouseY,
              bool mouseBtnDown = false);
  void click(ElementNode *root, float mouseX, float mouseY);

  bool isCaptured() const { return m_captured != nullptr; }

private:
  ElementNode *hitTest(ElementNode *node, float mx, float my) const;
  ElementNode *m_prevHover{nullptr};
  ElementNode *m_captured{nullptr};
  bool m_prevMouseDown{false};
};

} // namespace RaidenUI
