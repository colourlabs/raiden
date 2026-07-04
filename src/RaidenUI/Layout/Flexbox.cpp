#include <RaidenUI/Layout/Flexbox.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace RaidenUI {

namespace {

std::string_view trimmed(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }

  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
    s.remove_suffix(1);
  }

  return s;
}

std::vector<std::string_view> split(std::string_view s, char delim) {
  std::vector<std::string_view> parts;
  size_t start = 0;

  while (true) {
    size_t end = s.find(delim, start);
    parts.push_back(trimmed(s.substr(start, end - start)));

    if (end == std::string_view::npos) {
      break;
    }

    start = end + 1;
  }

  return parts;
}

} // namespace

Length parseLength(const std::string &str) {
  std::string_view sv = trimmed(str);

  if (sv == "auto") {
    return {.value = 0, .unit = Unit::Auto};
  }
  if (sv == "0") {
    return {.value = 0, .unit = Unit::Px};
  }

  size_t i = 0;
  bool hasDot = false;

  if (i < sv.size() && sv[i] == '-') {
    ++i;
  }

  while (i < sv.size() &&
         (std::isdigit(static_cast<unsigned char>(sv[i])) != 0 ||
          (sv[i] == '.' && !hasDot))) {
    if (sv[i] == '.') {
      hasDot = true;
    }
    ++i;
  }

  float value = 0;
  std::from_chars(sv.data(), sv.data() + i, value);

  std::string_view unitStr = trimmed(sv.substr(i));

  if (unitStr == "%") {
    return {.value = value, .unit = Unit::Percent};
  }

  return {.value = value, .unit = Unit::Px};
}

BoxEdges parseBoxShorthand(const std::string &str) {
  BoxEdges edges{};
  auto parts = split(str, ' ');

  if (parts.empty()) {
    return edges;
  }

  auto parseEdge = [](std::string_view s) -> float {
    Length len = parseLength(std::string(s));
    return (len.unit == Unit::Auto) ? 0.0F : len.value;
  };

  switch (parts.size()) {
  case 1: {
    float v = parseEdge(parts[0]);
    edges.top = edges.right = edges.bottom = edges.left = v;
    break;
  }
  case 2: {
    float vert = parseEdge(parts[0]);
    float horz = parseEdge(parts[1]);
    edges.top = edges.bottom = vert;
    edges.right = edges.left = horz;
    break;
  }
  case 3: {
    edges.top = parseEdge(parts[0]);
    float horz = parseEdge(parts[1]);
    edges.bottom = parseEdge(parts[2]);
    edges.right = edges.left = horz;
    break;
  }
  default: {
    edges.top = parseEdge(parts[0]);
    edges.right = parseEdge(parts[1]);
    edges.bottom = parseEdge(parts[2]);
    edges.left = parseEdge(parts[3]);
    break;
  }
  }

  return edges;
}

FlexStyle parseFlexStyle(const ComputedStyle &style) {
  FlexStyle fs;

  {
    auto it = style.find("flex");
    if (it != style.end()) {
      auto parts = split(it->second, ' ');
      if (!parts.empty()) {
        float v = 0;
        std::from_chars(parts[0].data(), parts[0].data() + parts[0].size(), v);
        fs.flexGrow = v;
      }
    }
  }

  auto it = style.find("display");
  if (it != style.end()) {
    if (it->second == "flex") {
      fs.display = 1;
    } else if (it->second == "none") {
      fs.display = -1;
    }
  }

  it = style.find("flex-direction");
  if (it != style.end()) {
    if (it->second == "column") {
      fs.flexDirection = FlexDirection::Column;
    } else if (it->second == "row-reverse") {
      fs.flexDirection = 2;
    } else if (it->second == "column-reverse") {
      fs.flexDirection = 3;
    }
  }

  it = style.find("justify-content");

  if (it != style.end()) {
    if (it->second == "center") {
      fs.justifyContent = JustifyContent::Center;
    } else if (it->second == "flex-end") {
      fs.justifyContent = JustifyContent::FlexEnd;
    } else if (it->second == "space-between") {
      fs.justifyContent = JustifyContent::SpaceBetween;
    } else if (it->second == "space-around") {
      fs.justifyContent = JustifyContent::SpaceAround;
    }
  }

  it = style.find("align-items");
  if (it != style.end()) {
    if (it->second == "center") {
      fs.alignItems = AlignItems::Center;
    } else if (it->second == "flex-end") {
      fs.alignItems = AlignItems::FlexEnd;
    } else if (it->second == "stretch") {
      fs.alignItems = AlignItems::Stretch;
    }
  }

  it = style.find("flex-grow");
  if (it != style.end()) {
    float v = 0;
    std::from_chars(it->second.data(), it->second.data() + it->second.size(),
                    v);
    fs.flexGrow = v;
  }

  it = style.find("flex-shrink");
  if (it != style.end()) {
    float v = 1;
    std::from_chars(it->second.data(), it->second.data() + it->second.size(),
                    v);
    fs.flexShrink = v;
  }

  it = style.find("gap");
  if (it != style.end()) {
    fs.gap = parseLength(it->second).value;
  }

  it = style.find("width");
  if (it != style.end()) {
    fs.width = parseLength(it->second);
  }

  it = style.find("height");
  if (it != style.end()) {
    fs.height = parseLength(it->second);
  }

  it = style.find("min-width");
  if (it != style.end()) {
    fs.minWidth = parseLength(it->second).value;
  }

  it = style.find("min-height");
  if (it != style.end()) {
    fs.minHeight = parseLength(it->second).value;
  }

  it = style.find("margin");
  if (it != style.end()) {
    fs.margin = parseBoxShorthand(it->second);
  }

  it = style.find("padding");
  if (it != style.end()) {
    fs.padding = parseBoxShorthand(it->second);
  }

  return fs;
}

LayoutSize layoutFlexContainer(ElementNode *container,
                               const FlexStyle &flexStyle,
                               const CssStylesheet &stylesheet,
                               const MeasureFn &measure) {
  struct Item {
    ElementNode *node;
    FlexStyle childStyle;
    float baseMain{0};
    float baseCross{0};
    float flexGrow{0};
    float flexShrink{0};
  };

  std::vector<Item> items;
  for (auto &child : container->children) {
    ComputedStyle cs = resolveStyle(child.get(), stylesheet);
    FlexStyle childFs = parseFlexStyle(cs);
    if (childFs.display < 0) {
      continue;
    }
    items.push_back(
        {child.get(), childFs, 0, 0, childFs.flexGrow, childFs.flexShrink});
  }

  bool isRow = (flexStyle.flexDirection == FlexDirection::Row);

  float padL = flexStyle.padding.left;
  float padR = flexStyle.padding.right;
  float padT = flexStyle.padding.top;
  float padB = flexStyle.padding.bottom;

  float innerW = std::max(0.0F, container->computedWidth - padL - padR);
  float innerH = std::max(0.0F, container->computedHeight - padT - padB);

  for (auto &item : items) {
    Length &mainLen = isRow ? item.childStyle.width : item.childStyle.height;
    Length &crossLen = isRow ? item.childStyle.height : item.childStyle.width;

    bool needsMeasure =
        measure && isMeasurableLeaf(item.node) &&
        (mainLen.unit == Unit::Auto || crossLen.unit == Unit::Auto);

    LayoutSize measured{};
    if (needsMeasure) {
      float avail = isRow ? innerW : innerH;
      measured = measure(item.node, avail);
    }

    if (mainLen.unit == Unit::Px) {
      item.baseMain = mainLen.value;
    } else if (mainLen.unit == Unit::Percent) {
      float ref = isRow ? container->computedWidth : container->computedHeight;
      item.baseMain = mainLen.value * 0.01F * ref;
    } else if (needsMeasure) {
      item.baseMain = isRow ? measured.width : measured.height;
    } else {
      item.baseMain = 0;
    }

    if (crossLen.unit == Unit::Px) {
      item.baseCross = crossLen.value;
    } else if (crossLen.unit == Unit::Percent) {
      float ref = isRow ? container->computedHeight : container->computedWidth;
      item.baseCross = crossLen.value * 0.01F * ref;
    } else if (needsMeasure) {
      item.baseCross = isRow ? measured.height : measured.width;
    } else {
      item.baseCross = 0;
    }

    // clamp base main/cross to min-width / min-height
    {
      float minMain =
          isRow ? item.childStyle.minWidth : item.childStyle.minHeight;
      item.baseMain = std::max(minMain, item.baseMain);
    }

    {
      float minCross =
          isRow ? item.childStyle.minHeight : item.childStyle.minWidth;
      item.baseCross = std::max(minCross, item.baseCross);
    }

    if (isRow) {
      item.baseMain +=
          item.childStyle.margin.left + item.childStyle.margin.right;
    } else {
      item.baseMain +=
          item.childStyle.margin.top + item.childStyle.margin.bottom;
    }
  }

  int count = static_cast<int>(items.size());
  float totalGap = flexStyle.gap * static_cast<float>(std::max(count - 1, 0));
  float totalBase = 0;
  for (auto &item : items) {
    totalBase += item.baseMain;
  }

  float available = (isRow ? innerW : innerH) - totalBase - totalGap;

  if (available > 0) {
    float totalGrow = 0;
    for (auto &item : items) {
      totalGrow += item.flexGrow;
    }
    if (totalGrow > 0) {
      float remain = available;
      for (int i = 0; i < count; ++i) {
        float share =
            items[static_cast<size_t>(i)].flexGrow / totalGrow * available;
        if (i == count - 1) {
          share = remain;
        }
        items[static_cast<size_t>(i)].baseMain += share;
        remain -= share;
      }
    }
  } else if (available < 0) {
    float totalWeight = 0;
    for (auto &item : items) {
      totalWeight += item.flexShrink * item.baseMain;
    }
    if (totalWeight > 0) {
      float deficit = -available;
      for (auto &item : items) {
        float shrink = item.flexShrink * item.baseMain / totalWeight * deficit;
        item.baseMain = std::max(0.0F, item.baseMain - shrink);
      }
    }
  }

  float maxCross = 0;
  for (auto &item : items) {
    float crossRef = isRow ? innerH : innerW;
    float marginCross =
        isRow ? (item.childStyle.margin.top + item.childStyle.margin.bottom)
              : (item.childStyle.margin.left + item.childStyle.margin.right);
    if (flexStyle.alignItems == AlignItems::Stretch && crossRef > marginCross) {
      item.baseCross = std::max(item.baseCross, crossRef - marginCross);
    }
    maxCross = std::max(maxCross, item.baseCross);
  }

  float containerCross = isRow ? innerH : innerW;
  float totalMain = totalBase + totalGap;
  float freeMain = std::max(0.0F, (isRow ? innerW : innerH) - totalMain);

  float mainCursor = 0;
  switch (flexStyle.justifyContent) {
  case JustifyContent::FlexEnd:
    mainCursor = freeMain;
    break;
  case JustifyContent::Center:
    mainCursor = freeMain * 0.5F;
    break;
  case JustifyContent::SpaceAround:
    mainCursor = freeMain / static_cast<float>(count * 2);
    break;
  default:
    break;
  }

  float extraBetween = 0;
  float extraAround = 0;
  if (flexStyle.justifyContent == JustifyContent::SpaceBetween && count > 1) {
    extraBetween = freeMain / static_cast<float>(count - 1);
  }
  if (flexStyle.justifyContent == JustifyContent::SpaceAround) {
    extraAround = freeMain / static_cast<float>(count);
  }

  for (int i = 0; i < count; ++i) {
    auto &item = items[static_cast<size_t>(i)];
    float ms = item.baseMain;
    float cs = item.baseCross;

    float crossOffset = 0;
    float crossSize = cs;
    float marginCross =
        isRow ? (item.childStyle.margin.top + item.childStyle.margin.bottom)
              : (item.childStyle.margin.left + item.childStyle.margin.right);

    if (flexStyle.alignItems == AlignItems::Center) {
      crossOffset = (containerCross - crossSize - marginCross) * 0.5F;
    } else if (flexStyle.alignItems == AlignItems::FlexEnd) {
      crossOffset = containerCross - crossSize - marginCross;
    } else if (flexStyle.alignItems == AlignItems::Stretch) {
      crossSize = std::max(crossSize, containerCross - marginCross);
    }
    crossOffset = std::max(0.0F, crossOffset);
    crossSize = std::max(0.0F, crossSize);

    if (isRow) {
      item.node->computedX = container->computedX + padL + mainCursor + item.childStyle.margin.left;
      item.node->computedY = container->computedY + padT + crossOffset + item.childStyle.margin.top;
      item.node->computedWidth =
          std::max(0.0F, ms - item.childStyle.margin.left -
                             item.childStyle.margin.right);
      item.node->computedHeight = std::max(0.0F, crossSize);
    } else {
      item.node->computedX = container->computedX + padL + crossOffset + item.childStyle.margin.left;
      item.node->computedY = container->computedY + padT + mainCursor + item.childStyle.margin.top;
      item.node->computedWidth = std::max(0.0F, crossSize);
      item.node->computedHeight =
          std::max(0.0F, ms - item.childStyle.margin.top -
                             item.childStyle.margin.bottom);
    }

    mainCursor += ms + flexStyle.gap + extraBetween + extraAround;
    if (i == 0 && flexStyle.justifyContent == JustifyContent::SpaceAround) {
      mainCursor += extraAround;
    }
  }

  float contentW = padL + padR + (isRow ? mainCursor : maxCross);
  float contentH = padT + padB + (isRow ? maxCross : mainCursor);
  contentW = std::max(0.0F, contentW);
  contentH = std::max(0.0F, contentH);

  return {.width = contentW, .height = contentH};
}

} // namespace RaidenUI
