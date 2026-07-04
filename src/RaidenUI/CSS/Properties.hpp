#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace RaidenUI {

using ComputedStyle = std::unordered_map<std::string, std::string>;

uint32_t parseCssColor(const std::string &str);
void expandShorthands(ComputedStyle &style);

} // namespace RaidenUI
