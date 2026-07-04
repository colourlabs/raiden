#pragma once

#include <RaidenUI/DOM/Element.hpp>

#include <memory>
#include <string_view>

namespace RaidenUI {

struct XmlDocument {
  std::unique_ptr<ElementNode> root;
};

XmlDocument parseXml(std::string_view source);

} // namespace RaidenUI
