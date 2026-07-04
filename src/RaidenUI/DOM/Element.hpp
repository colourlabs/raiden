#pragma once

#include <RaidenUI/Core/Signal.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RaidenUI {

struct ElementNode {
  std::string tag;

  std::unordered_map<std::string, std::string> attrs;
  std::unordered_map<std::string, std::string> style;
  std::vector<std::unique_ptr<ElementNode>> children;

  ElementNode *parent{nullptr};

  std::function<void()> onClick;
  std::function<void()> onMouseEnter;
  std::function<void()> onMouseLeave;

  std::string content;

  float computedX{0};
  float computedY{0};
  float computedWidth{0};
  float computedHeight{0};
  bool visible{true};
  bool hovered{false};

  explicit ElementNode(std::string tag) : tag(std::move(tag)) {}
};

} // namespace RaidenUI
