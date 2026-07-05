#pragma once

#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/DOM/Element.hpp>
#include <RaidenUI/Layout/Flexbox.hpp>

namespace RaidenUI {

void computeLayout(ElementNode *root, const CssStylesheet &stylesheet,
                   float viewportWidth, float viewportHeight,
                   const MeasureFn &measure);

} // namespace RaidenUI
