#include <RaidenUI/Layout/Flexbox.hpp>

#include <cmath>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
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
      if (parts.size() >= 2) {
        float v = 0;
        std::from_chars(parts[1].data(), parts[1].data() + parts[1].size(), v);
        fs.flexShrink = v;
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
    } else if (it->second == "flex-start") {
      fs.alignItems = AlignItems::FlexStart;
    }
  }

  it = style.find("align-self");
  if (it != style.end()) {
    if (it->second == "center") {
      fs.alignSelf = AlignItems::Center;
    } else if (it->second == "flex-end") {
      fs.alignSelf = AlignItems::FlexEnd;
    } else if (it->second == "stretch") {
      fs.alignSelf = AlignItems::Stretch;
    } else if (it->second == "flex-start") {
      fs.alignSelf = AlignItems::FlexStart;
    }
  }

  it = style.find("flex-wrap");
  if (it != style.end()) {
    if (it->second == "wrap") {
      fs.flexWrap = FlexWrap::Wrap;
    } else if (it->second == "wrap-reverse") {
      fs.flexWrap = FlexWrap::WrapReverse;
    }
  }

  it = style.find("align-content");
  if (it != style.end()) {
    if (it->second == "center") {
      fs.alignContent = AlignContent::Center;
    } else if (it->second == "flex-end") {
      fs.alignContent = AlignContent::FlexEnd;
    } else if (it->second == "space-between") {
      fs.alignContent = AlignContent::SpaceBetween;
    } else if (it->second == "space-around") {
      fs.alignContent = AlignContent::SpaceAround;
    } else if (it->second == "stretch") {
      fs.alignContent = AlignContent::Stretch;
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

  // individual margin properties override shorthand
  it = style.find("margin-top");
  if (it != style.end()) {
    fs.margin.top = parseLength(it->second).value;
  }
  it = style.find("margin-right");
  if (it != style.end()) {
    fs.margin.right = parseLength(it->second).value;
  }
  it = style.find("margin-bottom");
  if (it != style.end()) {
    fs.margin.bottom = parseLength(it->second).value;
  }
  it = style.find("margin-left");
  if (it != style.end()) {
    fs.margin.left = parseLength(it->second).value;
  }

  // individual padding properties override shorthand
  it = style.find("padding-top");
  if (it != style.end()) {
    fs.padding.top = parseLength(it->second).value;
  }
  it = style.find("padding-right");
  if (it != style.end()) {
    fs.padding.right = parseLength(it->second).value;
  }
  it = style.find("padding-bottom");
  if (it != style.end()) {
    fs.padding.bottom = parseLength(it->second).value;
  }
  it = style.find("padding-left");
  if (it != style.end()) {
    fs.padding.left = parseLength(it->second).value;
  }

  return fs;
}

namespace {

struct SizeCacheKey {
  const ElementNode *node;
  float availableMain;
  float availableCross;
  bool parentIsRow;

  bool operator==(const SizeCacheKey &other) const {
    return node == other.node && availableMain == other.availableMain &&
           availableCross == other.availableCross &&
           parentIsRow == other.parentIsRow;
  }
};

struct SizeCacheKeyHash {
  size_t operator()(const SizeCacheKey &k) const {
    size_t h = std::hash<const ElementNode *>{}(k.node);
    h ^= std::hash<float>{}(k.availableMain) << 1;
    h ^= std::hash<float>{}(k.availableCross) << 2;
    h ^= std::hash<bool>{}(k.parentIsRow) << 3;
    return h;
  }
};

std::unordered_map<SizeCacheKey, LayoutSize, SizeCacheKeyHash>
    g_minMaxCache; // NOLINT

struct LayoutCacheEntry {
  float x{0};
  float y{0};
  float width{0};
  float height{0};
  LayoutSize contentResult;
};

std::unordered_map<const ElementNode *, LayoutCacheEntry>
    g_layoutCache; // NOLINT

} // namespace

void clearLayoutCache() {
  g_minMaxCache.clear();
  g_layoutCache.clear();
}

MinMaxSizes computeMinMaxSizes(const ElementNode *node, const FlexStyle &style,
                               const CssStylesheet &stylesheet,
                               const MeasureFn &measure,
                               const ComputedStyle *parentStyle,
                               float availableMain, float availableCross,
                               bool parentIsRow) {
  SizeCacheKey key{.node = node,
                   .availableMain = availableMain,
                   .availableCross = availableCross,
                   .parentIsRow = parentIsRow};
  if (!node->needsLayout && !node->styleDirty) {
    auto cached = g_minMaxCache.find(key);
    if (cached != g_minMaxCache.end()) {
      return {cached->second};
    }
  }

  LayoutSize natural{};

  if (isMeasurableLeaf(node)) {
    if (measure) {
      natural = measure(node, availableMain);
    }
  } else if (!node->children.empty()) {
    bool isRow = (style.flexDirection == FlexDirection::Row);

    float ownWidth = parentIsRow ? availableMain : availableCross;
    float ownHeight = parentIsRow ? availableCross : availableMain;

    float padL = style.padding.left;
    float padR = style.padding.right;
    float padT = style.padding.top;
    float padB = style.padding.bottom;

    float innerW = std::max(0.0F, ownWidth - padL - padR);
    float innerH = std::max(0.0F, ownHeight - padT - padB);

    float childAvailMain = isRow ? innerW : innerH;
    float childAvailCross = isRow ? innerH : innerW;

    float mainTotal = 0;
    float crossMax = 0;
    int count = 0;

    for (const auto &childPtr : node->children) {
      ElementNode *child = childPtr.get();
      const ComputedStyle &childCS =
          resolveStyle(child, stylesheet, parentStyle);
      FlexStyle childFs = parseFlexStyle(childCS);
      if (childFs.display < 0) {
        continue;
      }
      ++count;

      Length &mainLen = isRow ? childFs.width : childFs.height;
      Length &crossLen = isRow ? childFs.height : childFs.width;

      bool needsChildMeasure =
          (mainLen.unit != Unit::Px) || (crossLen.unit != Unit::Px);

      LayoutSize childNatural{};
      if (needsChildMeasure) {
        MinMaxSizes childSizes =
            computeMinMaxSizes(child, childFs, stylesheet, measure, parentStyle,
                               childAvailMain, childAvailCross, isRow);
        childNatural = childSizes.naturalSize;
      }

      float childMain =
          (mainLen.unit == Unit::Px)
              ? mainLen.value
              : (isRow ? childNatural.width : childNatural.height);
      float childCross =
          (crossLen.unit == Unit::Px)
              ? crossLen.value
              : (isRow ? childNatural.height : childNatural.width);

      if (mainLen.unit != Unit::Px) {
        childMain += isRow ? (childFs.padding.left + childFs.padding.right)
                           : (childFs.padding.top + childFs.padding.bottom);
      }

      if (crossLen.unit != Unit::Px) {
        childCross += isRow ? (childFs.padding.top + childFs.padding.bottom)
                            : (childFs.padding.left + childFs.padding.right);
      }

      float minMain = isRow ? childFs.minWidth : childFs.minHeight;
      float minCross = isRow ? childFs.minHeight : childFs.minWidth;
      childMain = std::max(childMain, minMain);
      childCross = std::max(childCross, minCross);

      if (isRow) {
        childMain += childFs.margin.left + childFs.margin.right;
        childCross += childFs.margin.top + childFs.margin.bottom;
      } else {
        childMain += childFs.margin.top + childFs.margin.bottom;
        childCross += childFs.margin.left + childFs.margin.right;
      }
      mainTotal += childMain;
      crossMax = std::max(crossMax, childCross);
    }

    mainTotal += style.gap * static_cast<float>(std::max(count - 1, 0));

    natural.width = isRow ? mainTotal : crossMax;
    natural.height = isRow ? crossMax : mainTotal;
  }

  if (style.width.unit == Unit::Px) {
    natural.width = style.width.value;
  }
  if (style.height.unit == Unit::Px) {
    natural.height = style.height.value;
  }

  natural.width = std::max(natural.width, style.minWidth);
  natural.height = std::max(natural.height, style.minHeight);

  g_minMaxCache[key] = natural;
  return {natural};
}

LayoutSize layoutFlexContainer(ElementNode *container,
                               const FlexStyle &flexStyle,
                               const CssStylesheet &stylesheet,
                               const MeasureFn &measure,
                               const ComputedStyle *parentStyle,
                               ConstraintSpace /*constraintSpace*/) {
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
    const ComputedStyle &cs =
        resolveStyle(child.get(), stylesheet, parentStyle);
    FlexStyle childFs = parseFlexStyle(cs);
    if (childFs.display < 0) {
      continue;
    }
    items.push_back(
        {child.get(), childFs, 0, 0, childFs.flexGrow, childFs.flexShrink});
  }

  bool isRow = (flexStyle.flexDirection == FlexDirection::Row ||
                flexStyle.flexDirection == 2);
  bool isReversed =
      (flexStyle.flexDirection == 2 || flexStyle.flexDirection == 3);

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
        (mainLen.unit == Unit::Auto || crossLen.unit == Unit::Auto);

    LayoutSize measured{};
    if (needsMeasure) {
      float availMain = isRow ? innerW : innerH;
      float availCross = isRow ? innerH : innerW;

      MinMaxSizes sizes =
          computeMinMaxSizes(item.node, item.childStyle, stylesheet, measure,
                             parentStyle, availMain, availCross, isRow);
      measured = sizes.naturalSize;
    }

    if (mainLen.unit == Unit::Px) {
      item.baseMain = mainLen.value;
    } else if (mainLen.unit == Unit::Percent) {
      float ref = isRow ? container->computedWidth : container->computedHeight;
      item.baseMain = mainLen.value * 0.01F * ref;
    } else if (needsMeasure) {
      item.baseMain = isRow ? measured.width + item.childStyle.padding.left +
                                  item.childStyle.padding.right
                            : measured.height + item.childStyle.padding.top +
                                  item.childStyle.padding.bottom;
    } else {
      item.baseMain = 0;
    }

    if (crossLen.unit == Unit::Px) {
      item.baseCross = crossLen.value;
    } else if (crossLen.unit == Unit::Percent) {
      float ref = isRow ? container->computedHeight : container->computedWidth;
      item.baseCross = crossLen.value * 0.01F * ref;
    } else if (needsMeasure) {
      item.baseCross = isRow ? measured.height + item.childStyle.padding.top +
                                   item.childStyle.padding.bottom
                             : measured.width + item.childStyle.padding.left +
                                   item.childStyle.padding.right;
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

  // Helpers shared across line processing
  auto layoutLine = [&](const std::vector<size_t> &indices, float availableMain,
                        float lineCrossOrigin, float lineCrossSize)
      -> std::pair<float /*usedMain*/, float /*grossCross*/> {
    int count = static_cast<int>(indices.size());
    if (count == 0) {
      return {0, 0};
    }

    // totalBase for this line
    float totalBase = 0;
    for (size_t idx : indices) {
      totalBase += items[idx].baseMain;
    }
    float totalGap = flexStyle.gap * static_cast<float>(std::max(count - 1, 0));

    // flex-grow / flex-shrink within line
    float available = availableMain - totalBase - totalGap;
    if (available > 0) {
      float totalGrow = 0;
      for (size_t idx : indices) {
        totalGrow += items[idx].flexGrow;
      }
      if (totalGrow > 0) {
        float remain = available;
        for (int i = 0; i < count; ++i) {
          auto &item = items[indices[static_cast<size_t>(i)]];
          float share = item.flexGrow / totalGrow * available;
          if (i == count - 1) {
            share = remain;
          }
          item.baseMain += share;
          remain -= share;
        }
      }
    } else if (available < 0) {
      float totalWeight = 0;
      for (size_t idx : indices) {
        auto &item = items[idx];
        totalWeight += item.flexShrink * item.baseMain;
      }
      if (totalWeight > 0) {
        float deficit = -available;
        for (size_t idx : indices) {
          auto &item = items[idx];
          float shrink =
              item.flexShrink * item.baseMain / totalWeight * deficit;
          item.baseMain = std::max(0.0F, item.baseMain - shrink);
        }
      }
    }

    float totalMain = 0;
    for (size_t idx : indices) {
      totalMain += items[idx].baseMain;
    }
    totalMain += totalGap;

    // stretch: cross size reference is the line's cross size
    for (size_t idx : indices) {
      auto &item = items[idx];
      float marginCross =
          isRow ? (item.childStyle.margin.top + item.childStyle.margin.bottom)
                : (item.childStyle.margin.left + item.childStyle.margin.right);
      int effectiveAlign = item.childStyle.alignSelf >= 0
                               ? item.childStyle.alignSelf
                               : flexStyle.alignItems;
      if (effectiveAlign == AlignItems::Stretch) {
        Length &crossLen =
            isRow ? item.childStyle.height : item.childStyle.width;
        if (crossLen.unit == Unit::Auto && lineCrossSize > marginCross) {
          item.baseCross =
              std::max(item.baseCross, lineCrossSize - marginCross);
        }
      }
    }

    // justify-content cursor
    float freeMain = std::max(0.0F, availableMain - totalMain);
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

    // --- position items ---
    for (int i = 0; i < count; ++i) {
      auto &item = items[indices[static_cast<size_t>(i)]];
      float ms = item.baseMain;
      float cs = item.baseCross;

      int effectiveAlign = item.childStyle.alignSelf >= 0
                               ? item.childStyle.alignSelf
                               : flexStyle.alignItems;

      float crossOffset = 0;
      float crossSize = cs;
      float marginCross =
          isRow ? (item.childStyle.margin.top + item.childStyle.margin.bottom)
                : (item.childStyle.margin.left + item.childStyle.margin.right);

      if (effectiveAlign == AlignItems::Center) {
        crossOffset = (lineCrossSize - crossSize - marginCross) * 0.5F;
      } else if (effectiveAlign == AlignItems::FlexEnd) {
        crossOffset = lineCrossSize - crossSize - marginCross;
      }
      crossOffset = std::max(0.0F, crossOffset);
      crossSize = std::max(0.0F, crossSize);

      if (isRow) {
        float x = NAN;
        
        if (isReversed) {
          float visualW =
              ms - item.childStyle.margin.left - item.childStyle.margin.right;
          x = container->computedX + container->computedWidth - padR -
              mainCursor - item.childStyle.margin.right - visualW;
        } else {
          x = container->computedX + padL + mainCursor +
              item.childStyle.margin.left;
        }
        float y = container->computedY + padT + lineCrossOrigin + crossOffset +
                  item.childStyle.margin.top;
        item.node->computedX = x;
        item.node->computedY = y;
        item.node->computedWidth =
            std::max(0.0F, ms - item.childStyle.margin.left -
                               item.childStyle.margin.right);
        item.node->computedHeight = std::max(0.0F, crossSize);
      } else {
        float y = NAN;
        if (isReversed) {
          float visualH =
              ms - item.childStyle.margin.top - item.childStyle.margin.bottom;
          y = container->computedY + container->computedHeight - padB -
              mainCursor - item.childStyle.margin.bottom - visualH;
        } else {
          y = container->computedY + padT + mainCursor +
              item.childStyle.margin.top;
        }
        float x = container->computedX + padL + lineCrossOrigin + crossOffset +
                  item.childStyle.margin.left;
        item.node->computedX = x;
        item.node->computedY = y;
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

    return {totalMain, lineCrossSize};
  };

  float availableMain = isRow ? innerW : innerH;
  float containerCross = isRow ? innerH : innerW;

  // split items into lines
  struct FlexLine {
    std::vector<size_t> indices;
    float crossSize{0};
  };
  std::vector<FlexLine> lines;

  if (flexStyle.flexWrap == FlexWrap::NoWrap || items.empty()) {
    FlexLine line;
    for (size_t i = 0; i < items.size(); ++i) {
      line.indices.push_back(i);
    }
    if (!items.empty()) {
      line.crossSize = containerCross;
      lines.push_back(std::move(line));
    }
  } else {
    FlexLine cur;
    float curMain = 0;
    float curCross = 0;
    float gap = flexStyle.gap;
    for (size_t i = 0; i < items.size(); ++i) {
      float im = items[i].baseMain;
      float gapDelta = cur.indices.empty() ? 0 : gap;
      if (curMain + gapDelta + im > availableMain && !cur.indices.empty()) {
        cur.crossSize = curCross;
        lines.push_back(std::move(cur));
        cur = FlexLine{};
        curMain = 0;
        curCross = 0;
        gapDelta = 0;
      }
      cur.indices.push_back(i);
      curMain += gapDelta + im;
      curCross = std::max(curCross, items[i].baseCross);
    }
    if (!cur.indices.empty()) {
      cur.crossSize = curCross;
      lines.push_back(std::move(cur));
    }
  }

  // layout each line
  float totalCrossUsed = 0;
  float maxMainUsed = 0;
  float lineGap = flexStyle.gap;

  for (auto &line : lines) {
    auto [usedMain, grossCross] =
        layoutLine(line.indices, availableMain, totalCrossUsed, line.crossSize);
    line.crossSize = grossCross;
    totalCrossUsed += grossCross;
    maxMainUsed = std::max(maxMainUsed, usedMain);
  }

  // align-content: distribute lines in the cross axis
  if (lines.size() > 1) {
    float totalGaps = lineGap * static_cast<float>(lines.size() - 1);
    float freeCross = containerCross - totalCrossUsed - totalGaps;

    if (freeCross > 0 && flexStyle.alignContent != AlignContent::FlexStart) {
      auto lineAdjust = [&](size_t lineIndex, float) -> float {
        switch (flexStyle.alignContent) {
        case AlignContent::FlexEnd:
          return freeCross;
        case AlignContent::Center:
          return freeCross * 0.5F;
        case AlignContent::SpaceBetween:
          return freeCross / static_cast<float>(lines.size() - 1) *
                 static_cast<float>(lineIndex);
        case AlignContent::SpaceAround:
          return freeCross / static_cast<float>(lines.size()) *
                 (0.5F + static_cast<float>(lineIndex));
        case AlignContent::Stretch:
          return freeCross / static_cast<float>(lines.size()) *
                 static_cast<float>(lineIndex);
        default:
          return 0;
        }
      };

      float crossOrigin = 0;
      for (size_t li = 0; li < lines.size(); ++li) {
        float adjust = lineAdjust(li, lines[li].crossSize);
        if (flexStyle.alignContent == AlignContent::Stretch) {
          float extra = freeCross / static_cast<float>(lines.size());
          for (size_t idx : lines[li].indices) {
            auto &item = items[idx];
            if (lines[li].crossSize > 0) {
              float scale = (lines[li].crossSize + extra) / lines[li].crossSize;
              item.baseCross *= scale;
            }
          }
          lines[li].crossSize += extra;
        }
        layoutLine(lines[li].indices, availableMain, crossOrigin + adjust,
                   lines[li].crossSize);
        crossOrigin += lines[li].crossSize + lineGap;
      }

      totalCrossUsed = 0;
      for (auto &line : lines) {
        totalCrossUsed += line.crossSize;
      }
      totalCrossUsed += totalGaps;

      maxMainUsed = 0;
      for (auto &line : lines) {
        for (size_t idx : line.indices) {
          float marginMain = isRow ? (items[idx].childStyle.margin.left +
                                      items[idx].childStyle.margin.right)
                                   : (items[idx].childStyle.margin.top +
                                      items[idx].childStyle.margin.bottom);
          maxMainUsed = std::max(maxMainUsed, items[idx].baseMain - marginMain);
        }
      }
    } else if (freeCross > 0) {
      float crossOrigin = 0;
      for (auto &line : lines) {
        layoutLine(line.indices, availableMain, crossOrigin, line.crossSize);
        crossOrigin += line.crossSize + lineGap;
      }
    }
  }

  // recurse children
  for (const auto &item : items) {
    if (item.node->children.empty()) {
      continue;
    }

    auto cacheIt = g_layoutCache.find(item.node);
    bool canReuse = !item.node->needsLayout && !item.node->styleDirty &&
                    cacheIt != g_layoutCache.end() &&
                    cacheIt->second.x == item.node->computedX &&
                    cacheIt->second.y == item.node->computedY &&
                    cacheIt->second.width == item.node->computedWidth &&
                    cacheIt->second.height == item.node->computedHeight;

    if (canReuse) {
      continue;
    }

    const ComputedStyle &childStyle =
        resolveStyle(item.node, stylesheet, parentStyle);
    FlexStyle childFs = parseFlexStyle(childStyle);

    LayoutSize childResult = layoutFlexContainer(item.node, childFs, stylesheet,
                                                 measure, &childStyle);

    g_layoutCache[item.node] = {.x = item.node->computedX,
                                .y = item.node->computedY,
                                .width = item.node->computedWidth,
                                .height = item.node->computedHeight,
                                .contentResult = childResult};
    item.node->needsLayout = false;
  }

  // return content size
  float contentCross = NAN;
  if (flexStyle.flexWrap != FlexWrap::NoWrap && lines.size() > 1) {
    contentCross = totalCrossUsed;
  } else {
    contentCross = 0;
    for (const auto &item : items) {
      contentCross = std::max(contentCross, isRow ? item.node->computedHeight
                                                  : item.node->computedWidth);
    }
  }

  float contentW = padL + padR + (isRow ? maxMainUsed : contentCross);
  float contentH = padT + padB + (isRow ? contentCross : maxMainUsed);
  contentW = std::max(0.0F, contentW);
  contentH = std::max(0.0F, contentH);

  return {.width = contentW, .height = contentH};
}

} // namespace RaidenUI