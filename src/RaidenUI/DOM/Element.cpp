#include <RaidenUI/DOM/Element.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace RaidenUI {

ElementNode::ElementNode(std::string tag_) : tag(std::move(tag_)) {}

void ElementNode::appendChild(std::unique_ptr<ElementNode> child) {
  child->parent = this;
  children.push_back(std::move(child));
}

void ElementNode::removeChild(const ElementNode *child) {
  auto it = std::ranges::find_if(
      children, [child](const auto &ptr) { return ptr.get() == child; });
  if (it != children.end()) {
    (*it)->parent = nullptr;
    children.erase(it);
  }
}

void ElementNode::setAttribute(const std::string &name, std::string value) {
  attrs[name] = std::move(value);
}

const std::string *ElementNode::getAttribute(
    const std::string &name) const {
  auto it = attrs.find(name);
  if (it != attrs.end()) {
    return &it->second;
  }
  return nullptr;
}

bool ElementNode::hasAttribute(const std::string &name) const {
  return attrs.contains(name);
}

void ElementNode::removeAttribute(const std::string &name) {
  attrs.erase(name);
}

void ElementNode::setStyle(const std::string &prop, std::string value) {
  style[prop] = std::move(value);
}

ElementNode *ElementNode::getElementById(const std::string &id) {
  auto it = attrs.find("id");
  if (it != attrs.end() && it->second == id) {
    return this;
  }
  for (const auto &child : children) {
    ElementNode *found = child->getElementById(id);
    if (found) {
      return found;
    }
  }
  return nullptr;
}

std::vector<ElementNode *> ElementNode::getElementsByClassName(
    const std::string &className) const {
  std::vector<ElementNode *> result;
  auto it = attrs.find("class");
  if (it != attrs.end()) {
    size_t start = 0;
    while (true) {
      size_t end = it->second.find(' ', start);
      std::string_view token =
          (end == std::string::npos)
              ? std::string_view(it->second).substr(start)
              : std::string_view(it->second).substr(start, end - start);
      if (token == className) {
        result.push_back(const_cast<ElementNode *>(this));
        break;
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
  }
  for (const auto &child : children) {
    auto childResults = child->getElementsByClassName(className);
    result.insert(result.end(), childResults.begin(), childResults.end());
  }
  return result;
}

std::string ElementNode::textContent() const {
  std::string result = content;
  for (const auto &child : children) {
    result += child->textContent();
  }
  return result;
}

std::unique_ptr<ElementNode> ElementNode::clone(bool deep) const {
  auto n = std::make_unique<ElementNode>(tag);
  n->attrs = attrs;
  n->style = style;
  n->content = content;
  n->computedX = computedX;
  n->computedY = computedY;
  n->computedWidth = computedWidth;
  n->computedHeight = computedHeight;
  n->visible = visible;
  n->hovered = hovered;
  if (deep) {
    for (const auto &child : children) {
      n->appendChild(child->clone(true));
    }
  }
  return n;
}

ElementNode *ElementNode::firstChild() const {
  if (children.empty()) {
    return nullptr;
  }
  return children.front().get();
}

ElementNode *ElementNode::lastChild() const {
  if (children.empty()) {
    return nullptr;
  }
  return children.back().get();
}

size_t ElementNode::childCount() const { return children.size(); }

} // namespace RaidenUI
