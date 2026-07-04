#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace RaidenUI {

struct CssRule {
  std::string selector;
  std::vector<std::pair<std::string, std::string>> declarations;
};

struct CssStylesheet {
  std::vector<CssRule> rules;
};

CssStylesheet parseCss(std::string_view source);

} // namespace RaidenUI
