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

  explicit ElementNode(std::string tag);

  void appendChild(std::unique_ptr<ElementNode> child);
  void removeChild(const ElementNode *child);

  void setAttribute(const std::string &name, std::string value);
  const std::string *getAttribute(const std::string &name) const;
  bool hasAttribute(const std::string &name) const;
  void removeAttribute(const std::string &name);

  void setStyle(const std::string &prop, std::string value);

  ElementNode *getElementById(const std::string &id);
  std::vector<ElementNode *> getElementsByClassName(
      const std::string &className) const;
  std::string textContent() const;
  std::unique_ptr<ElementNode> clone(bool deep = false) const;

  ElementNode *firstChild() const;
  ElementNode *lastChild() const;
  size_t childCount() const;
};

} // namespace RaidenUI
