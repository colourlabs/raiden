#include <RaidenUI/CSS/Properties.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace RaidenUI {

namespace {

uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

uint32_t parseHexColor(std::string_view sv) {
  if (!sv.empty() && sv[0] == '#') {
    sv.remove_prefix(1);
  }

  auto hexVal = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') {
      return static_cast<uint8_t>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
      return static_cast<uint8_t>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
      return static_cast<uint8_t>(c - 'A' + 10);
    }
    return 0;
  };

  if (sv.size() == 3) {
    auto r = static_cast<uint8_t>(hexVal(sv[0]) * 17);
    auto g = static_cast<uint8_t>(hexVal(sv[1]) * 17);
    auto b = static_cast<uint8_t>(hexVal(sv[2]) * 17);
    return packRGBA(r, g, b);
  }

  if (sv.size() >= 6) {
    auto r = static_cast<uint8_t>((hexVal(sv[0]) * 16) + hexVal(sv[1]));
    auto g = static_cast<uint8_t>((hexVal(sv[2]) * 16) + hexVal(sv[3]));
    auto b = static_cast<uint8_t>((hexVal(sv[4]) * 16) + hexVal(sv[5]));
    return packRGBA(r, g, b);
  }

  return 0xFFFFFFFF;
}

} // namespace

uint32_t parseCssColor(const std::string &str) {
  std::string_view sv(str);

  while (!sv.empty() && sv.front() == ' ') {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && sv.back() == ' ') {
    sv.remove_suffix(1);
  }

  if (sv.empty()) {
    return 0;
  }

  if (sv[0] == '#') {
    return parseHexColor(sv);
  }

  if (sv == "transparent") {
    return 0;
  }
  if (sv == "red") {
    return packRGBA(255, 0, 0);
  }
  if (sv == "green") {
    return packRGBA(0, 128, 0);
  }
  if (sv == "blue") {
    return packRGBA(0, 0, 255);
  }
  if (sv == "white") {
    return packRGBA(255, 255, 255);
  }

  if (sv == "black") {
    return packRGBA(0, 0, 0);
  }

  if (sv == "gray" || sv == "grey") {
    return packRGBA(128, 128, 128);
  }

  if (sv == "yellow") {
    return packRGBA(255, 255, 0);
  }

  if (sv == "orange") {
    return packRGBA(255, 165, 0);
  }

  if (sv == "purple") {
    return packRGBA(128, 0, 128);
  }

  if (sv == "pink") {
    return packRGBA(255, 192, 203);
  }

  if (sv == "cyan") {
    return packRGBA(0, 255, 255);
  }

  if (sv == "magenta") {
    return packRGBA(255, 0, 255);
  }

  if (sv == "silver") {
    return packRGBA(192, 192, 192);
  }

  if (sv == "maroon") {
    return packRGBA(128, 0, 0);
  }

  if (sv == "olive") {
    return packRGBA(128, 128, 0);
  }

  if (sv == "navy") {
    return packRGBA(0, 0, 128);
  }

  if (sv == "teal") {
    return packRGBA(0, 128, 128);
  }

  if (sv == "aqua") {
    return packRGBA(0, 255, 255);
  }

  if (sv == "fuchsia") {
    return packRGBA(255, 0, 255);
  }

  if (sv == "lime") {
    return packRGBA(0, 255, 0);
  }

  return 0;
}

namespace {

std::vector<std::string> splitValue(const std::string &val) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (true) {
    while (start < val.size() && val[start] == ' ') {
      ++start;
    }
    if (start >= val.size()) {
      break;
    }
    size_t end = val.find(' ', start);
    if (end == std::string::npos) {
      parts.push_back(val.substr(start));
      break;
    }
    parts.push_back(val.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

bool isLengthToken(const std::string &t) {
  if (t == "0" || t == "thin" || t == "medium" || t == "thick") {
    return true;
  }
  if (t.size() > 2) {
    std::string_view sv(t);
    if (sv.ends_with("px") || sv.ends_with("em") || sv.ends_with("rem") ||
        sv.ends_with("pt") || sv.ends_with("vh") || sv.ends_with("vw") ||
        sv.ends_with("%")) {
      return true;
    }
  }
  return false;
}

bool isStyleKeyword(const std::string &t) {
  return t == "none" || t == "hidden" || t == "dotted" || t == "dashed" ||
         t == "solid" || t == "double" || t == "groove" || t == "ridge" ||
         t == "inset" || t == "outset";
}

bool isColorToken(const std::string &t) {
  if (t.empty()) {
    return false;
  }
  if (t[0] == '#') {
    return true;
  }
  if (t == "transparent" || t == "red" || t == "green" || t == "blue" ||
      t == "white" || t == "black" || t == "gray" || t == "grey" ||
      t == "yellow" || t == "orange" || t == "purple" || t == "pink" ||
      t == "cyan" || t == "magenta" || t == "silver" || t == "maroon" ||
      t == "olive" || t == "navy" || t == "teal" || t == "aqua" ||
      t == "fuchsia" || t == "lime") {
    return true;
  }
  return false;
}

void expandBorderValue(const std::string &side, const std::string &val,
                       ComputedStyle &style) {
  std::vector<std::string> parts = splitValue(val);
  std::string width, styleKW, color;

  for (const auto &p : parts) {
    if (isStyleKeyword(p)) {
      styleKW = p;
    } else if (isColorToken(p)) {
      color = p;
    } else if (isLengthToken(p)) {
      width = p;
    }
  }

  if (!width.empty()) {
    style[side + "-width"] = width;
  }
  if (!styleKW.empty()) {
    style[side + "-style"] = styleKW;
  }
  if (!color.empty()) {
    style[side + "-color"] = color;
  }
}

void expandBorder(const std::string &val, ComputedStyle &style) {
  expandBorderValue("border-top", val, style);
  expandBorderValue("border-right", val, style);
  expandBorderValue("border-bottom", val, style);
  expandBorderValue("border-left", val, style);
}

void expandMarginPadding(const std::string &prop, const std::string &val,
                         ComputedStyle &style) {
  std::vector<std::string> parts = splitValue(val);
  if (parts.size() == 1) {
    style[prop + "-top"] = parts[0];
    style[prop + "-right"] = parts[0];
    style[prop + "-bottom"] = parts[0];
    style[prop + "-left"] = parts[0];
  } else if (parts.size() == 2) {
    style[prop + "-top"] = parts[0];
    style[prop + "-right"] = parts[1];
    style[prop + "-bottom"] = parts[0];
    style[prop + "-left"] = parts[1];
  } else if (parts.size() == 3) {
    style[prop + "-top"] = parts[0];
    style[prop + "-right"] = parts[1];
    style[prop + "-bottom"] = parts[2];
    style[prop + "-left"] = parts[1];
  } else if (parts.size() >= 4) {
    style[prop + "-top"] = parts[0];
    style[prop + "-right"] = parts[1];
    style[prop + "-bottom"] = parts[2];
    style[prop + "-left"] = parts[3];
  }
}

} // namespace

void expandShorthands(ComputedStyle &style) {
  std::vector<std::pair<std::string, std::string>> entries;
  for (const auto &kv : style) {
    entries.emplace_back(kv);
  }

  for (const auto &[prop, val] : entries) {
    if (prop == "background") {
      std::vector<std::string> parts = splitValue(val);
      for (const auto &p : parts) {
        if (isColorToken(p)) {
          style["background-color"] = p;
          break;
        }
      }
    } else if (prop == "border") {
      expandBorder(val, style);
    } else if (prop == "border-top" || prop == "border-right" ||
               prop == "border-bottom" || prop == "border-left") {
      expandBorderValue(prop, val, style);
    } else if (prop == "margin") {
      expandMarginPadding("margin", val, style);
    } else if (prop == "padding") {
      expandMarginPadding("padding", val, style);
    } else if (prop == "border-width") {
      style["border-top-width"] = val;
      style["border-right-width"] = val;
      style["border-bottom-width"] = val;
      style["border-left-width"] = val;
    } else if (prop == "border-style") {
      style["border-top-style"] = val;
      style["border-right-style"] = val;
      style["border-bottom-style"] = val;
      style["border-left-style"] = val;
    } else if (prop == "border-color") {
      style["border-top-color"] = val;
      style["border-right-color"] = val;
      style["border-bottom-color"] = val;
      style["border-left-color"] = val;
    }
  }
}

} // namespace RaidenUI
