#include <RaidenEditor/Widgets/ProjectLauncherDialog.hpp>
#include <RaidenEditor/Widgets/NewProjectDialog.hpp>

#include <RaidenEditor/Core/ProjectDiscovery.hpp>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace RaidenEditor {

ProjectLauncherDialog::ProjectLauncherDialog(QWidget *parent)
    : QDialog(parent),
      gamesDir_(ProjectDiscovery::defaultGamesDir()), recentList_(new QListWidget()), allList_(new QListWidget()) {
  setWindowTitle(QStringLiteral("Raiden Editor"));
  setMinimumSize(520, 400);
  setObjectName(QStringLiteral("ProjectLauncherDialog"));

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setSpacing(12);
  mainLayout->setContentsMargins(20, 20, 20, 16);

  // title
  auto *title = new QLabel(QStringLiteral("Raiden Editor"));
  title->setObjectName(QStringLiteral("LauncherTitle"));
  mainLayout->addWidget(title);

  // recent projects
  recentHeader_ = new QLabel(QStringLiteral("Recent Projects"));
  recentHeader_->setObjectName(QStringLiteral("SectionHeader"));
  mainLayout->addWidget(recentHeader_);
  
  recentList_->setObjectName(QStringLiteral("ProjectList"));
  recentList_->setMaximumHeight(140);
  connect(recentList_, &QListWidget::itemDoubleClicked, this,
          &ProjectLauncherDialog::onItemDoubleClicked);
  mainLayout->addWidget(recentList_);

  // all projects
  allHeader_ = new QLabel(QStringLiteral("All Projects"));
  allHeader_->setObjectName(QStringLiteral("SectionHeader"));
  mainLayout->addWidget(allHeader_);

  allList_->setObjectName(QStringLiteral("ProjectList"));
  connect(allList_, &QListWidget::itemDoubleClicked, this,
          &ProjectLauncherDialog::onItemDoubleClicked);
  mainLayout->addWidget(allList_, 1);

  // bottom row: buttons
  auto *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(8);

  auto *newBtn = new QPushButton(QStringLiteral("  New Project"));
  newBtn->setObjectName(QStringLiteral("NewProjectButton"));
  newBtn->setCursor(Qt::PointingHandCursor);
  connect(newBtn, &QPushButton::clicked, this,
          &ProjectLauncherDialog::onNewProjectClicked);
  bottomRow->addWidget(newBtn);

  bottomRow->addStretch(1);

  auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"));
  refreshBtn->setObjectName(QStringLiteral("RefreshButton"));
  refreshBtn->setCursor(Qt::PointingHandCursor);
  connect(refreshBtn, &QPushButton::clicked, this,
          &ProjectLauncherDialog::onRefreshClicked);
  bottomRow->addWidget(refreshBtn);

  auto *openBtn = new QPushButton(QStringLiteral("Open"));
  openBtn->setObjectName(QStringLiteral("OpenButton"));
  openBtn->setCursor(Qt::PointingHandCursor);
  openBtn->setEnabled(false);
  connect(openBtn, &QPushButton::clicked, this,
          &ProjectLauncherDialog::onOpenClicked);
  bottomRow->addWidget(openBtn);

  mainLayout->addLayout(bottomRow);

  // enable open when something is selected
  auto enableOpen = [this, openBtn]() {
    auto *active = recentList_->currentItem();
    if (active == nullptr) {
      active = allList_->currentItem();
    }
    openBtn->setEnabled(active != nullptr);
  };

  connect(recentList_, &QListWidget::currentItemChanged, this, enableOpen);
  connect(allList_, &QListWidget::currentItemChanged, this, enableOpen);

  populateLists();
}

void ProjectLauncherDialog::populateLists() {
  recentList_->clear();
  allList_->clear();

  auto projects = ProjectDiscovery::scanDirectory(gamesDir_);
  auto recent = ProjectDiscovery::loadRecent();

  // build path -> project map
  std::unordered_map<std::string, ProjectInfo> projectMap;
  for (const auto &p : projects) {
    projectMap[p.path] = p;
  }

  // populate recent list
  int recentCount = 0;
  for (const auto &path : recent) {
    auto it = projectMap.find(path);
    if (it != projectMap.end()) {
      recentList_->addItem(createItem(it->second));
      recentCount++;
    }
  }

  recentHeader_->setVisible(recentCount > 0);
  recentList_->setVisible(recentCount > 0);

  // populate all projects list
  for (const auto &p : projects) {
    allList_->addItem(createItem(p));
  }

  if (projects.empty()) {
    auto *emptyItem = new QListWidgetItem(
        QStringLiteral("No projects found in '%1'")
            .arg(QString::fromStdString(gamesDir_)));
    emptyItem->setFlags(Qt::NoItemFlags);
    allList_->addItem(emptyItem);
  }
}

QListWidgetItem *
ProjectLauncherDialog::createItem(const ProjectInfo &info) { // NOLINT
  QString label = QString::fromStdString(info.name);
  if (info.hasPlugin) {
    label += QStringLiteral("  (with plugin)");
  }

  auto *item = new QListWidgetItem(label);
  item->setData(Qt::UserRole, QString::fromStdString(info.path));
  item->setToolTip(QString::fromStdString(info.path));
  return item;
}

void ProjectLauncherDialog::onItemDoubleClicked(QListWidgetItem *item) {
  if (item == nullptr || !item->flags().testFlag(Qt::ItemIsSelectable)) {
    return;
  }
  selectedPath_ = item->data(Qt::UserRole).toString().toStdString();
  ProjectDiscovery::saveRecent(selectedPath_);
  accept();
}

void ProjectLauncherDialog::onOpenClicked() {
  auto *active = recentList_->currentItem();
  if (active == nullptr) {
    active = allList_->currentItem();
  }
  if (active == nullptr || !active->flags().testFlag(Qt::ItemIsSelectable)) {
    return;
  }
  selectedPath_ = active->data(Qt::UserRole).toString().toStdString();
  ProjectDiscovery::saveRecent(selectedPath_);
  accept();
}

void ProjectLauncherDialog::onNewProjectClicked() {
  NewProjectDialog dlg(gamesDir_, this);
  if (dlg.exec() == QDialog::Accepted) {
    auto newPath = dlg.createdProjectPath();
    if (!newPath.empty()) {
      ProjectDiscovery::saveRecent(newPath);
      populateLists();
    }
  }
}

void ProjectLauncherDialog::onRefreshClicked() { populateLists(); }

} // namespace RaidenEditor
