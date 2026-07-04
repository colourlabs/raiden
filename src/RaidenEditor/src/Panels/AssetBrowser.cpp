#include <RaidenEditor/Panels/AssetBrowser.hpp>

#include <RaidenUI/DOM/Element.hpp>

namespace RaidenEditor {

AssetBrowser::AssetBrowser() = default;
AssetBrowser::~AssetBrowser() = default;

std::unique_ptr<RaidenUI::ElementNode> AssetBrowser::build() {
  auto panel = std::make_unique<RaidenUI::ElementNode>("div");
  panel->style["display"] = "flex";
  panel->style["flex-direction"] = "column";
  panel->style["min-width"] = "250px";
  panel->content = "Asset Browser";

  for (const auto &entry : entries_) {
    auto item = std::make_unique<RaidenUI::ElementNode>("div");
    item->style["padding"] = "4px 8px";
    item->style["cursor"] = "pointer";
    item->content = entry;
    panel->children.push_back(std::move(item));
  }

  return panel;
}

void AssetBrowser::navigateTo(const std::string &path) {
  currentPath_ = path;
}

} // namespace RaidenEditor
