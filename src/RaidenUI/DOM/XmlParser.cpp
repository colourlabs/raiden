#include <RaidenUI/DOM/XmlParser.hpp>

#include <pugixml.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RaidenUI {

namespace {

std::unique_ptr<ElementNode> convertNode(const pugi::xml_node &xmlNode) {
  if (!xmlNode) {
    return nullptr;
  }

  std::string tag = xmlNode.name();
  if (tag.empty()) {
    return nullptr;
  }

  auto node = std::make_unique<ElementNode>(std::move(tag));

  // attributes
  for (const auto &attr : xmlNode.attributes()) {
    std::string name = attr.name();
    std::string value = attr.value();

    if (name == "class") {
      node->attrs["class"] = value;
    } else if (name == "id") {
      node->attrs["id"] = value;
    } else if (name == "style") {
      // parse CSS-like inline styles
      size_t start = 0;

      while (true) {
        size_t colon = value.find(':', start);
        if (colon == std::string::npos) {
          break;
        }

        std::string prop = value.substr(start, colon - start);

        // trim
        while (!prop.empty() && prop.front() == ' ') {
          prop.erase(prop.begin());
        }

        while (!prop.empty() && prop.back() == ' ') {
          prop.pop_back();
        }

        start = colon + 1;
        size_t semi = value.find(';', start);
        std::string val = value.substr(start, semi - start);

        while (!val.empty() && val.front() == ' ') {
          val.erase(val.begin());
        }

        while (!val.empty() && val.back() == ' ') {
          val.pop_back();
        }

        node->style[prop] = val;

        if (semi == std::string::npos) {
          break;
        }

        start = semi + 1;
      }
    } else {
      node->attrs[name] = value;
    }
  }

  // children
  for (const auto &child : xmlNode.children()) {
    if (child.type() == pugi::node_element) {
      auto childNode = convertNode(child);

      if (childNode) {
        childNode->parent = node.get();
        node->children.push_back(std::move(childNode));
      }
    } else if (child.type() == pugi::node_pcdata) {
      // text node, store in content
      std::string text = child.value();

      while (!text.empty() && (text.front() == ' ' || text.front() == '\n' ||
                               text.front() == '\t')) {
        text.erase(text.begin());
      }

      while (!text.empty() && (text.back() == ' ' || text.back() == '\n' ||
                               text.back() == '\t')) {
        text.pop_back();
      }

      if (!text.empty()) {
        node->content = text;
      }
    }
  }

  return node;
}

} // namespace

XmlDocument parseXml(std::string_view source) {
  XmlDocument doc;

  pugi::xml_document xmlDoc;
  pugi::xml_parse_result result = xmlDoc.load_string(
      source.data(), pugi::parse_default | pugi::parse_cdata);

  if (!result) {
    return doc;
  }

  pugi::xml_node rootXml = xmlDoc.document_element();
  doc.root = convertNode(rootXml);

  return doc;
}

} // namespace RaidenUI
