#pragma once

#include <RaidenUI/Layout/Flexbox.hpp>
#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/DOM/Element.hpp>

namespace RaidenUI {

void computeLayout(ElementNode *root, const CssStylesheet &stylesheet,
                   float viewportWidth, float viewportHeight,
                   const MeasureFn &measure);

} // namespace RaidenUI
