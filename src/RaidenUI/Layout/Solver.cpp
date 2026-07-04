#include <RaidenUI/Layout/Solver.hpp>

#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/Layout/Flexbox.hpp>
#include <algorithm>

namespace RaidenUI {

namespace {

// Pre-order pass: propagate explicit sizes (px/percent) top-down so that
// children know their container's size before post-order layout begins.
void layoutElement_preorder(ElementNode *node,
                            const CssStylesheet &stylesheet) {
  ComputedStyle style = resolveStyle(node, stylesheet);
  FlexStyle fs = parseFlexStyle(style);

  if (fs.display < 0) {
    node->computedWidth = 0;
    node->computedHeight = 0;
    node->visible = false;
    return;
  }

  if (fs.width.unit == Unit::Px) {
    node->computedWidth = fs.width.value;
  } else if (fs.width.unit == Unit::Percent && node->parent != nullptr) {
    node->computedWidth = fs.width.value * 0.01F * node->parent->computedWidth;
  }

  if (fs.height.unit == Unit::Px) {
    node->computedHeight = fs.height.value;
  } else if (fs.height.unit == Unit::Percent && node->parent != nullptr) {
    node->computedHeight =
        fs.height.value * 0.01F * node->parent->computedHeight;
  }

  for (auto &child : node->children) {
    layoutElement_preorder(child.get(), stylesheet);
  }
}

// Post-order layout pass.
// parentSized: true when this node's size was already set by its parent's flex
//   layout, so auto-size overwrite should be skipped.
void layoutElement(ElementNode *node, const CssStylesheet &stylesheet,
                   bool parentSized = false) {
  ComputedStyle style = resolveStyle(node, stylesheet);
  FlexStyle fs = parseFlexStyle(style);

  if (fs.display == 1) {
    LayoutSize content = layoutFlexContainer(node, fs, stylesheet);
    for (auto &child : node->children) {
      if (child->visible) {
        layoutElement(child.get(), stylesheet, true);
      }
    }
    if (!parentSized) {
      if (fs.width.unit == Unit::Auto) {
        node->computedWidth = content.width;
      }
      if (fs.height.unit == Unit::Auto) {
        node->computedHeight = content.height;
      }
    }
  } else if (fs.display < 0) {
    node->computedWidth = 0;
    node->computedHeight = 0;
    node->visible = false;
  } else {
    for (auto &child : node->children) {
      if (child->visible) {
        layoutElement(child.get(), stylesheet, false);
      }
    }
    float y = fs.padding.top;
    float maxW = 0;
    for (auto &child : node->children) {
      if (!child->visible) {
        continue;
      }
      child->computedX = fs.padding.left;
      child->computedY = y;
      y += child->computedHeight;
      maxW = std::max(maxW, child->computedX + child->computedWidth);
    }
    if (!parentSized) {
      if (fs.width.unit == Unit::Auto) {
        node->computedWidth = maxW + fs.padding.right;
      }
      if (fs.height.unit == Unit::Auto) {
        node->computedHeight = y + fs.padding.bottom;
      }
    }
  }
}

} // namespace

void computeLayout(ElementNode *root, const CssStylesheet &stylesheet,
                   float viewportWidth, float viewportHeight) {
  root->computedWidth = viewportWidth;
  root->computedHeight = viewportHeight;
  layoutElement_preorder(root, stylesheet);
  layoutElement(root, stylesheet);
}

} // namespace RaidenUI
