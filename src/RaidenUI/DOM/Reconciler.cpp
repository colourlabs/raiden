#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/DOM/Reconciler.hpp>

#include <algorithm>

namespace RaidenUI {

namespace {

void reconcileChildren(ElementNode *live, ElementNode *next) {
  size_t oldCount = live->children.size();
  size_t newCount = next->children.size();
  size_t common = std::min(oldCount, newCount);

  // reconcile existing children pairwise
  for (size_t i = 0; i < common; ++i) {
    reconcile(live->children[i].get(), next->children[i].get());
  }

  // remove surplus old children
  if (oldCount > newCount) {
    live->children.resize(newCount);
  }

  // move new children into the live tree
  if (newCount > oldCount) {
    for (size_t i = oldCount; i < newCount; ++i) {
      next->children[i]->parent = live;
      live->children.push_back(std::move(next->children[i]));
    }
  }
}

} // namespace

void reconcile(ElementNode *live, ElementNode *next) {
  live->content = std::move(next->content);

  bool styleChanged =
      (live->attrs != next->attrs) || (live->style != next->style);

  live->attrs = std::move(next->attrs);
  live->style = std::move(next->style);

  live->onClick = std::move(next->onClick);
  live->onMouseDown = std::move(next->onMouseDown);
  live->onMouseUp = std::move(next->onMouseUp);
  live->onMouseEnter = std::move(next->onMouseEnter);
  live->onMouseLeave = std::move(next->onMouseLeave);

  if (styleChanged) {
    markStyleDirty(live);
    requestLayout(live);
  }

  // preserve runtime layout state
  // computedX/Y/Width/Height, visible, hovered are kept as-is

  reconcileChildren(live, next);
}

} // namespace RaidenUI
