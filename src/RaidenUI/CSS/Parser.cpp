#include <RaidenUI/CSS/Parser.hpp>

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

// ref:
// https://raw.githubusercontent.com/mikke89/RmlUi/master/Source/Core/StyleSheetParser.cpp

namespace RaidenUI {

namespace {

class Parser {
public:
  explicit Parser(std::string_view source) : m_source(source) {}

  CssStylesheet parse() {
    CssStylesheet sheet;
    skipWhitespace();

    while (!eof()) {
      if (peek() == '@') {
        skipAtRule();
        continue;
      }
    
      sheet.rules.push_back(parseRule());
      skipWhitespace();
    }

    return sheet;
  }

private:
  std::string_view m_source;
  size_t m_pos{0};
  int m_line{1};

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

    char c = m_source[m_pos++];
    if (c == '\n') {
      ++m_line;
    }

    return c;
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
      if (c == '/' && m_pos + 1 < m_source.size() &&
          m_source[m_pos + 1] == '*') {
        skipComment();
      } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') {
        advance();
      } else {
        break;
      }
    }
  }

  void skipComment() {
    if (!match('/') || !match('*')) {
      return;
    }

    while (!eof()) {
      if (advance() == '*' && !eof() && peek() == '/') {
        advance();
        return;
      }
    }
  }

  void skipAtRule() {
    while (!eof() && peek() != '{') {
      advance();
    }

    if (match('{')) {
      int depth = 1;
      while (depth > 0 && !eof()) {
        char c = advance();
        if (c == '{') {
          ++depth;
        } else if (c == '}') {
          --depth;
        }
      }
    }
  }

  CssRule parseRule() {
    CssRule rule;
    rule.selector = parseSelectorList();

    skipWhitespace();

    if (match('{')) {
      parseDeclarationBlock(rule);
      match('}');
    }
    return rule;
  }

  std::string parseSelectorList() {
    std::string result;
    bool first = true;

    while (true) {
      skipWhitespace();
      if (eof() || peek() == '{') {
        break;
      }

      std::string sel = parseSelector();
      if (!sel.empty()) {
        if (!first) {
          result += ", ";
        }
        result += sel;
        first = false;
      }

      skipWhitespace();
      if (match(',')) {
        continue;
      }

      break;
    }
    return result;
  }

  std::string parseSelector() {
    std::string sel;
    bool first = true;

    while (true) {
      skipWhitespace();
      if (eof() || peek() == '{' || peek() == ',' || peek() == '}') {
        break;
      }

      if (match('>') || match('+') || match('~')) {
        sel += ' ';
        sel += m_source[m_pos - 1];
        sel += ' ';
        first = true;
        skipWhitespace();
        continue;
      }

      if (!first) {
        sel += ' ';
      }

      std::string simple = parseSimpleSelector();
      sel += simple;
      first = false;
    }

    return sel;
  }

  std::string parseSimpleSelector() {
    std::string result;

    char c = peek();

    if (c == '*') {
      result += advance();
    } else if (c == '.') {
      result += advance();
      result += parseIdent();
    } else if (c == '#') {
      result += advance();
      result += parseIdent();
    } else if (c == ':') {
      result += advance();
      if (peek() == ':') {
        result += advance();
      }
      result += parseIdent();
      if (peek() == '(') {
        result += advance();
        while (!eof() && peek() != ')') {
          result += advance();
        }
        if (match(')')) {
          result += ')';
        }
      }
    } else if (c == '[') {
      result += advance();
      skipWhitespace();
      result += parseIdent();
      skipWhitespace();
      if (!eof() && (peek() == '=' || peek() == '~' || peek() == '|' ||
                     peek() == '^' || peek() == '$' || peek() == '*')) {
        result += advance();
        if (peek() == '=') {
          result += advance();
        }
        skipWhitespace();
        if (peek() == '"' || peek() == '\'') {
          result += parseString();
        } else {
          result += parseIdent();
        }
        skipWhitespace();
      }
      if (match(']')) {
        result += ']';
      }
    } else if (std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '-' ||
               c == '_') {
      result += parseIdent();
    }

    return result;
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

  std::string parseString() {
    std::string result;
    if (eof()) {
      return result;
    }

    char quote = advance();
    result += quote;

    while (!eof()) {
      char c = peek();
      if (c == '\\') {
        result += advance();
        if (!eof()) {
          result += advance();
        }
      } else if (c == quote) {
        result += advance();
        break;
      } else {
        result += advance();
      }
    }

    return result;
  }

  void parseDeclarationBlock(CssRule &rule) {
    while (true) {
      skipWhitespace();
      if (eof() || peek() == '}') {
        break;
      }

      std::string name = parseIdent();
      if (name.empty()) {
        while (!eof() && peek() != ';' && peek() != '}') {
          advance();
        }
        if (match(';')) {
          continue;
        }
        break;
      }

      skipWhitespace();
      if (!match(':')) {
        if (match(';')) {
          continue;
        }
        break;
      }
      skipWhitespace();

      std::string value = parseValue();

      skipWhitespace();
      match(';');

      rule.declarations.emplace_back(std::move(name), std::move(value));
    }
  }

  std::string parseValue() {
    std::string result;
    int parenDepth = 0;

    while (!eof()) {
      char c = peek();

      if (c == ';' && parenDepth == 0) {
        break;
      }
      if (c == '}' && parenDepth == 0) {
        break;
      }

      if (c == '(') {
        ++parenDepth;
        result += advance();
      } else if (c == ')') {
        --parenDepth;
        result += advance();
      } else if (c == '"' || c == '\'') {
        result += parseString();
      } else if (c == '/' && m_pos + 1 < m_source.size() &&
                 m_source[m_pos + 1] == '*') {
        skipComment();
      } else {
        result += advance();
      }
    }

    while (!result.empty() &&
           (result.back() == ' ' || result.back() == '\t' ||
            result.back() == '\n' || result.back() == '\r')) {
      result.pop_back();
    }

    return result;
  }
};

} // namespace

CssStylesheet parseCss(std::string_view source) {
  return Parser(source).parse();
}

} // namespace RaidenUI
