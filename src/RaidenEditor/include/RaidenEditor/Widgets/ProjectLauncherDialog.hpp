#pragma once

#include <RaidenEditor/Core/ProjectDiscovery.hpp>

#include <QDialog>

#include <string>
#include <vector>

class QListWidget;
class QListWidgetItem;
class QLabel;

namespace RaidenEditor {

class ProjectLauncherDialog : public QDialog {
  Q_OBJECT
public:
  explicit ProjectLauncherDialog(QWidget *parent = nullptr);

  [[nodiscard]] std::string selectedProjectPath() const {
    return selectedPath_;
  }

private slots:
  void onItemDoubleClicked(QListWidgetItem *item);
  void onOpenClicked();
  void onNewProjectClicked();
  void onRefreshClicked();

private:
  void populateLists();
  QListWidgetItem *createItem(const ProjectInfo &info);

  std::string gamesDir_;
  std::string selectedPath_;
  QListWidget *recentList_ = nullptr;
  QListWidget *allList_ = nullptr;
  QLabel *recentHeader_ = nullptr;
  QLabel *allHeader_ = nullptr;
};

} // namespace RaidenEditor
