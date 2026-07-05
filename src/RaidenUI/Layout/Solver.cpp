#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/Layout/Flexbox.hpp>
#include <RaidenUI/Layout/Solver.hpp>

#include <algorithm>

namespace RaidenUI {

namespace {

void layoutElement_preorder(ElementNode *node, const CssStylesheet &stylesheet,
                            const ComputedStyle *parentStyle = nullptr) {
  const ComputedStyle &style = resolveStyle(node, stylesheet, parentStyle);
  FlexStyle fs = parseFlexStyle(style);

  if (fs.display < 0) {
    node->computedWidth = 0;
    node->computedHeight = 0;
    node->visible = false;
    return;
  }

  if (fs.width.unit == Unit::Px) {
    node->computedWidth = fs.width.value;
  }

  if (fs.height.unit == Unit::Px) {
    node->computedHeight = fs.height.value;
  }

  for (auto &child : node->children) {
    layoutElement_preorder(child.get(), stylesheet, &style);
  }
}

void layoutElement(ElementNode *node, const CssStylesheet &stylesheet,
                   bool parentSized = false, const MeasureFn &measure = nullptr,
                   const ComputedStyle *parentStyle = nullptr) {
  const ComputedStyle &style = resolveStyle(node, stylesheet, parentStyle);
  FlexStyle fs = parseFlexStyle(style);

  if (!parentSized) {
    if (fs.width.unit == Unit::Percent && node->parent != nullptr) {
      node->computedWidth =
          fs.width.value * 0.01F * node->parent->computedWidth;
    }
    if (fs.height.unit == Unit::Percent && node->parent != nullptr) {
      node->computedHeight =
          fs.height.value * 0.01F * node->parent->computedHeight;
    }
  }

  if (fs.display == 1) {
    LayoutSize content =
        layoutFlexContainer(node, fs, stylesheet, measure, &style);
    for (auto &child : node->children) {
      if (child->visible) {
        layoutElement(child.get(), stylesheet, true, measure, &style);
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
        layoutElement(child.get(), stylesheet, false, measure, &style);
      }
    }

    if (isMeasurableLeaf(node) && measure && !parentSized) {
      LayoutSize sz = measure(
          node, (node->parent != nullptr) ? node->parent->computedWidth : 0);
      if (fs.width.unit == Unit::Auto) {
        node->computedWidth = sz.width + fs.padding.left + fs.padding.right;
      }
      if (fs.height.unit == Unit::Auto) {
        node->computedHeight = sz.height + fs.padding.top + fs.padding.bottom;
      }
    }

    float y = fs.padding.top;
    float maxW = 0;
    for (auto &child : node->children) {
      if (!child->visible) {
        continue;
      }

      child->computedX = node->computedX + fs.padding.left;
      child->computedY = node->computedY + y;
      y += child->computedHeight;
      maxW = std::max(maxW, child->computedX + child->computedWidth);
    }

    if (!parentSized && !isMeasurableLeaf(node)) {
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
                   float viewportWidth, float viewportHeight,
                   const MeasureFn &measure) {
  static float lastVw = 0, lastVh = 0;
  bool viewportChanged =
      (lastVw != viewportWidth) || (lastVh != viewportHeight);
  if (!root->needsLayout && !viewportChanged) {
    return;
  }
  lastVw = viewportWidth;
  lastVh = viewportHeight;
  root->needsLayout = false;

  root->computedWidth = viewportWidth;
  root->computedHeight = viewportHeight;

  layoutElement_preorder(root, stylesheet);
  layoutElement(root, stylesheet, false, measure);
}

} // namespace RaidenUI
