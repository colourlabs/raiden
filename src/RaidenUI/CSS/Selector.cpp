#include <RaidenUI/CSS/Selector.hpp>

#include <RaidenUI/DOM/Element.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RaidenUI {

// selector string parser
namespace {

class SelectorParser {
public:
  explicit SelectorParser(std::string_view source) : m_source(source) {}

  Selector parse() {
    Selector sel;

    while (true) {
      skipWhitespace();
      if (eof() || peek() == '{' || peek() == ',' || peek() == '}') {
        break;
      }

      if (match('>') || match('+') || match('~')) {
        sel.combinators.push_back(m_source[m_pos - 1]);
        skipWhitespace();
        sel.compounds.push_back(parseCompound());
      } else {
        if (!sel.compounds.empty()) {
          sel.combinators.push_back(' ');
        }
        sel.compounds.push_back(parseCompound());
      }
    }

    return sel;
  }

private:
  std::string_view m_source;
  size_t m_pos{0};

  [[nodiscard]] bool eof() const { return m_pos >= m_source.size(); }

  [[nodiscard]] char peek() const {
    if (eof()) {
      return '\0';
    }
    return m_source[m_pos];
  }

  char advance() {
    if (eof()) {
      return '\0';
    }
    return m_source[m_pos++];
  }

  bool match(char c) {
    if (peek() != c) {
      return false;
    }
    advance();
    return true;
  }

  void skipWhitespace() {
    while (!eof()) {
      char c = peek();
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
        advance();
      } else {
        break;
      }
    }
  }

  std::string parseIdent() {
    std::string result;
    while (!eof()) {
      char c = peek();
      if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' ||
          c == '_') {
        result += advance();
      } else {
        break;
      }
    }
    return result;
  }

  CompoundSelector parseCompound() {
    CompoundSelector compound;

    while (!eof()) {
      skipWhitespace();
      char c = peek();
      if (c == '{' || c == ',' || c == '}' || c == '\0') {
        break;
      }
      if (c == '>' || c == '+' || c == '~') {
        break;
      }
      if (c == ' ') {
        size_t saved = m_pos;
        skipWhitespace();
        char next = peek();
        if (next == '>' || next == '+' || next == '~' || next == ',' ||
            next == '{' || next == '}') {
          m_pos = saved;
          break;
        }
        m_pos = saved;
        break;
      }

      compound.simples.push_back(parseSimple());
    }

    return compound;
  }

  SimpleSelector parseSimple() {
    char c = peek();

    if (c == '*') {
      advance();
      return {.type = SimpleSelectorType::Tag, .value = "*"};
    }

    if (c == '.') {
      advance();
      return {.type = SimpleSelectorType::Class, .value = parseIdent()};
    }

    if (c == '#') {
      advance();
      return {.type = SimpleSelectorType::Id, .value = parseIdent()};
    }

    if (c == ':') {
      advance();
      if (peek() == ':') {
        advance();
      }
      std::string name = parseIdent();
      if (peek() == '(') {
        advance();
        int depth = 1;
        while (!eof() && depth > 0) {
          if (peek() == '(') {
            ++depth;
          } else if (peek() == ')') {
            --depth;
          }
          name += advance();
        }
      }
      return {.type = SimpleSelectorType::PseudoClass,
              .value = std::move(name)};
    }

    if (c == '[') {
      advance();
      skipWhitespace();
      std::string repr = "[";
      repr += parseIdent();
      skipWhitespace();
      if (!eof() && (peek() == '=' || peek() == '~' || peek() == '|' ||
                     peek() == '^' || peek() == '$' || peek() == '*')) {
        repr += advance();
        if (peek() == '=') {
          repr += advance();
        }
        skipWhitespace();
        if (peek() == '"' || peek() == '\'') {
          char q = advance();
          repr += q;
          while (!eof() && peek() != q) {
            repr += advance();
          }
          if (peek() == q) {
            repr += advance();
          }
        } else {
          repr += parseIdent();
        }
        skipWhitespace();
      }
      if (match(']')) {
        repr += ']';
      }
      return {.type = SimpleSelectorType::PseudoClass,
              .value = std::move(repr)};
    }

    return {.type = SimpleSelectorType::Tag, .value = parseIdent()};
  }
};

// matching helpers
bool matchesAttributeSelector(std::string_view repr,
                              const ElementNode *element) {
  if (repr.size() < 2) {
    return false;
  }
  repr = repr.substr(1, repr.size() - 2);

  // find operator position
  size_t opPos = std::string_view::npos;
  char opKind = 0;
  size_t opLen = 0;

  struct Op {
    std::string_view token;
    char kind;
  };
  for (const auto &[token, kind] :
       {Op{.token = "~=", .kind = '~'}, Op{.token = "|=", .kind = '|'},
        Op{.token = "^=", .kind = '^'}, Op{.token = "$=", .kind = '$'},
        Op{.token = "*=", .kind = '*'}, Op{.token = "=", .kind = '='}}) {
    opPos = repr.find(token);
    if (opPos != std::string_view::npos) {
      opKind = kind;
      opLen = token.size();
      break;
    }
  }

  if (opPos == std::string_view::npos) {
    return element->attrs.contains(std::string(repr));
  }

  std::string_view name = repr.substr(0, opPos);
  std::string_view value = repr.substr(opPos + opLen);

  if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'')) {
    value = value.substr(1, value.size() - 2);
  }

  auto it = element->attrs.find(std::string(name));
  if (it == element->attrs.end()) {
    return false;
  }
  std::string_view attrVal(it->second);

  switch (opKind) {
  case '=':
    return attrVal == value;
  case '~': {
    size_t start = 0;
    while (true) {
      size_t end = attrVal.find(' ', start);
      std::string_view tok = (end == std::string_view::npos)
                                 ? attrVal.substr(start)
                                 : attrVal.substr(start, end - start);
      if (tok == value) {
        return true;
      }
      if (end == std::string_view::npos) {
        break;
      }
      start = end + 1;
    }
    return false;
  }
  case '|':
    return attrVal == value ||
           (attrVal.size() > value.size() && attrVal[value.size()] == '-' &&
            attrVal.starts_with(value));
  case '^':
    return attrVal.starts_with(value);
  case '$':
    if (attrVal.size() < value.size()) {
      return false;
    }
    return attrVal.substr(attrVal.size() - value.size()) == value;
  case '*':
    return attrVal.find(value) != std::string_view::npos;
  default:
    return false;
  }
}

bool simpleMatches(const SimpleSelector &simple, const ElementNode *element) {
  switch (simple.type) {
  case SimpleSelectorType::Tag: {
    if (simple.value == "*") {
      return true;
    }
    return element->tag == simple.value;
  }
  case SimpleSelectorType::Class: {
    auto it = element->attrs.find("class");
    if (it == element->attrs.end()) {
      return false;
    }
    std::string_view cls(it->second);
    size_t start = 0;
    while (true) {
      size_t end = cls.find(' ', start);
      std::string_view token = (end == std::string_view::npos)
                                   ? cls.substr(start)
                                   : cls.substr(start, end - start);
      if (token == simple.value) {
        return true;
      }
      if (end == std::string_view::npos) {
        break;
      }
      start = end + 1;
    }
    return false;
  }
  case SimpleSelectorType::Id: {
    auto it = element->attrs.find("id");
    if (it == element->attrs.end()) {
      return false;
    }
    return it->second == simple.value;
  }
  case SimpleSelectorType::PseudoClass: {
    if (simple.value == "hover") {
      return element->hovered;
    }
    if (!simple.value.empty() && simple.value[0] == '[') {
      return matchesAttributeSelector(simple.value, element);
    }
    return false;
  }
  }
  return false;
}

bool compoundMatches(const CompoundSelector &compound,
                     const ElementNode *element) {
  return std::ranges::all_of(
      compound.simples,
      [element](const SimpleSelector &s) { return simpleMatches(s, element); });
}

bool checkLeftPart(const Selector &selector, size_t idx,
                   const ElementNode *element) {
  if (idx == 0) {
    return true;
  }

  char combinator = selector.combinators[idx - 1];
  const CompoundSelector &target = selector.compounds[idx - 1];

  switch (combinator) {
  case ' ': {
    const ElementNode *p = element->parent;
    while (p != nullptr) {
      if (compoundMatches(target, p)) {
        if (checkLeftPart(selector, idx - 1, p)) {
          return true;
        }
      }
      p = p->parent;
    }
    return false;
  }

  case '>': {
    const ElementNode *p = element->parent;
    if (p == nullptr) {
      return false;
    }
    if (!compoundMatches(target, p)) {
      return false;
    }
    return checkLeftPart(selector, idx - 1, p);
  }

  case '+': {
    if (element->parent == nullptr) {
      return false;
    }
    const auto &siblings = element->parent->children;
    int found = -1;
    for (size_t i = 0; i < siblings.size(); ++i) {
      if (siblings[i].get() == element) {
        found = static_cast<int>(i);
        break;
      }
    }
    if (found <= 0) {
      return false;
    }
    const ElementNode *prev = siblings[static_cast<size_t>(found - 1)].get();
    if (!compoundMatches(target, prev)) {
      return false;
    }
    return checkLeftPart(selector, idx - 1, prev);
  }

  case '~': {
    if (element->parent == nullptr) {
      return false;
    }
    const auto &siblings = element->parent->children;
    int found = -1;
    for (size_t i = 0; i < siblings.size(); ++i) {
      if (siblings[i].get() == element) {
        found = static_cast<int>(i);
        break;
      }
    }
    if (found <= 0) {
      return false;
    }
    for (int i = 0; i < found; ++i) {
      const ElementNode *prev = siblings[static_cast<size_t>(i)].get();
      if (compoundMatches(target, prev)) {
        if (checkLeftPart(selector, idx - 1, prev)) {
          return true;
        }
      }
    }
    return false;
  }

  default:
    return false;
  }
}

bool selectorMatchesFull(const Selector &selector, const ElementNode *element) {
  size_t rightmost = selector.compounds.size() - 1;
  if (!compoundMatches(selector.compounds[rightmost], element)) {
    return false;
  }
  return checkLeftPart(selector, rightmost, element);
}

} // namespace

// public API
Selector parseSelectorString(std::string_view sel) {
  return SelectorParser(sel).parse();
}

Specificity computeSpecificity(const Selector &selector) {
  Specificity spec;
  for (const auto &compound : selector.compounds) {
    for (const auto &simple : compound.simples) {
      switch (simple.type) {
      case SimpleSelectorType::Id:
        ++spec.a;
        break;
      case SimpleSelectorType::Class:
      case SimpleSelectorType::PseudoClass:
        ++spec.b;
        break;
      case SimpleSelectorType::Tag:
        ++spec.c;
        break;
      }
    }
  }
  return spec;
}

bool selectorMatches(const Selector &selector, const ElementNode *element) {
  return selectorMatchesFull(selector, element);
}

ComputedStyle resolveStyle(const ElementNode *element,
                           const CssStylesheet &stylesheet) {
  struct Match {
    const CssRule *rule;
    Specificity specificity;
    int sourceOrder;
  };

  std::vector<Match> matches;

  for (size_t i = 0; i < stylesheet.rules.size(); ++i) {
    const auto &rule = stylesheet.rules[i];
    Selector sel = parseSelectorString(rule.selector);
    if (selectorMatches(sel, element)) {
      matches.push_back({&rule, computeSpecificity(sel), static_cast<int>(i)});
    }
  }

  std::ranges::sort(matches, [](const Match &a, const Match &b) {
    if (a.specificity != b.specificity) {
      return a.specificity < b.specificity;
    }
    
    return a.sourceOrder < b.sourceOrder;
  });

  ComputedStyle result;

  for (const auto &[prop, val] : element->style) {
    result[prop] = val;
  }

  for (const auto &match : matches) {
    for (const auto &[prop, val] : match.rule->declarations) {
      result[prop] = val;
    }
  }

  return result;
}

} // namespace RaidenUI
