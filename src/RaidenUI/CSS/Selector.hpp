#pragma once

#include <RaidenUI/CSS/Parser.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace RaidenUI {

struct ElementNode;

enum class SimpleSelectorType : std::uint8_t {
  Tag,
  Class,
  Id,
  PseudoClass,
};

struct SimpleSelector {
  SimpleSelectorType type;
  std::string value;
};

struct CompoundSelector {
  std::vector<SimpleSelector> simples;
};

struct Selector {
  std::vector<CompoundSelector> compounds;
  std::vector<char> combinators;
};

struct Specificity {
  int a{0};
  int b{0};
  int c{0};

  auto operator<=>(const Specificity &) const = default;
};

using ComputedStyle = std::unordered_map<std::string, std::string>;

Selector parseSelectorString(std::string_view selector);

Specificity computeSpecificity(const Selector &selector);

bool selectorMatches(const Selector &selector, const ElementNode *element);

ComputedStyle resolveStyle(const ElementNode *element,
                           const CssStylesheet &stylesheet);

} // namespace RaidenUI
