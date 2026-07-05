#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace RaidenUI {

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

struct CssRule {
  std::string selector_text;
  Selector selector;
  std::vector<std::pair<std::string, std::string>> declarations;
};

struct CssStylesheet {
  std::vector<CssRule> rules;
};

CssStylesheet parseCss(std::string_view source);

} // namespace RaidenUI
