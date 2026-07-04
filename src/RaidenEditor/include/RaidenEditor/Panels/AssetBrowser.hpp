#pragma once

#include <memory>
#include <string>
#include <vector>

namespace RaidenUI {
struct ElementNode;
} // namespace RaidenUI

namespace RaidenEditor {

class AssetBrowser {
public:
  AssetBrowser();
  ~AssetBrowser();

  AssetBrowser(const AssetBrowser &) = delete;
  AssetBrowser &operator=(const AssetBrowser &) = delete;

  std::unique_ptr<RaidenUI::ElementNode> build();
  void navigateTo(const std::string &path);

private:
  std::string currentPath_;
  std::vector<std::string> entries_;
};

} // namespace RaidenEditor
