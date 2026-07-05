#pragma once

#include <RaidenUI/CSS/Parser.hpp>
#include <RaidenUI/CSS/Properties.hpp>

#include <string_view>

namespace RaidenUI {

struct ElementNode;

Selector parseSelectorString(std::string_view selector);

Specificity computeSpecificity(const Selector &selector);

bool selectorMatches(const Selector &selector, const ElementNode *element);

const ComputedStyle &resolveStyle(ElementNode *element,
                                  const CssStylesheet &stylesheet,
                                  const ComputedStyle *parentStyle = nullptr);

void inheritProperties(ComputedStyle &child, const ComputedStyle &parent);

void markStyleDirty(ElementNode *node);

} // namespace RaidenUI
