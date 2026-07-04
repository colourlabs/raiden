#pragma once

#include <RaidenUI/CSS/Selector.hpp>
#include <RaidenUI/DOM/Element.hpp>
#include <cstdint>

namespace RaidenUI {

struct LayoutSize {
  float width{0};
  float height{0};
};

enum class Unit: std::uint8_t { Px, Percent, Auto };

struct Length {
  float value{0};
  Unit unit{Unit::Auto};
};

Length parseLength(const std::string &str);

struct BoxEdges {
  float top{0};
  float right{0};
  float bottom{0};
  float left{0};
};

BoxEdges parseBoxShorthand(const std::string &str);

struct FlexDirection { enum : std::uint8_t { Row, Column }; };
struct JustifyContent { enum : std::uint8_t { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround }; };
struct AlignItems { enum : std::uint8_t { FlexStart, FlexEnd, Center, Stretch }; };

struct FlexStyle {
  int display{0}; // 0 = block, 1 = flex, -1 = none
  int flexDirection{FlexDirection::Row};
  int justifyContent{JustifyContent::FlexStart};
  int alignItems{AlignItems::Stretch};
  float flexGrow{0};
  float flexShrink{1};
  float gap{0};
  Length width;
  Length height;
  BoxEdges margin;
  BoxEdges padding;
};

FlexStyle parseFlexStyle(const ComputedStyle &style);

LayoutSize layoutFlexContainer(ElementNode *container,
                               const FlexStyle &flexStyle,
                               const CssStylesheet &stylesheet);

} // namespace RaidenUI
