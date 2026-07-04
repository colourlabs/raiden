#pragma once

#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/DOM/Element.hpp>

namespace RaidenUI {

void computeLayout(ElementNode *root, const CssStylesheet &stylesheet,
                   float viewportWidth = 800, float viewportHeight = 600);

} // namespace RaidenUI
