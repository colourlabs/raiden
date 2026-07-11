#include <RaidenEditor/Widgets/NewProjectDialog.hpp>

#include <Raiden/Logger.hpp>

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

static const Raiden::Core::Logger s_logger("NewProjectDialog");

namespace RaidenEditor {

NewProjectDialog::NewProjectDialog(std::string gamesDir, QWidget *parent)
    : QDialog(parent), nameEdit_(new QLineEdit()),
      locationEdit_(new QLineEdit()), gamesDir_(std::move(gamesDir)) {
  setWindowTitle(QStringLiteral("Create New Project"));
  setMinimumWidth(420);
  setObjectName(QStringLiteral("NewProjectDialog"));

  auto *layout = new QVBoxLayout(this);
  layout->setSpacing(12);
  layout->setContentsMargins(20, 20, 20, 20);

  auto *formLayout = new QFormLayout();
  formLayout->setSpacing(8);

  nameEdit_->setPlaceholderText(QStringLiteral("My Game"));
  nameEdit_->setObjectName(QStringLiteral("ProjectNameInput"));
  formLayout->addRow(QStringLiteral("Project Name:"), nameEdit_);

  auto *locationRow = new QWidget();
  auto *locationHl = new QHBoxLayout(locationRow);
  locationHl->setContentsMargins(0, 0, 0, 0);
  locationHl->setSpacing(4);

  locationEdit_->setText(QString::fromStdString(gamesDir_));
  locationEdit_->setReadOnly(true);
  locationEdit_->setObjectName(QStringLiteral("ProjectLocationInput"));
  locationHl->addWidget(locationEdit_);

  auto *browseBtn = new QPushButton(QStringLiteral("Browse..."));
  browseBtn->setObjectName(QStringLiteral("BrowseButton"));
  connect(browseBtn, &QPushButton::clicked, this, &NewProjectDialog::onBrowse);
  locationHl->addWidget(browseBtn);

  formLayout->addRow(QStringLiteral("Location:"), locationRow);

  layout->addLayout(formLayout);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Create"));
  buttons->button(QDialogButtonBox::Ok)
      ->setObjectName(QStringLiteral("CreateButton"));
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  connect(buttons, &QDialogButtonBox::accepted, this,
          &NewProjectDialog::onCreate);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  connect(nameEdit_, &QLineEdit::textChanged, this,
          &NewProjectDialog::validate);
}

void NewProjectDialog::onBrowse() {
  QString dir =
      QFileDialog::getExistingDirectory(this, QStringLiteral("Select Location"),
                                        QString::fromStdString(gamesDir_));
  if (!dir.isEmpty()) {
    locationEdit_->setText(dir);
  }
}

void NewProjectDialog::onCreate() {
  QString name = nameEdit_->text().trimmed();
  if (name.isEmpty()) {
    return;
  }

  QString location = locationEdit_->text();
  QString projectDir = location + "/" + name;

  if (QDir(projectDir).exists()) {
    QMessageBox::warning(this, QStringLiteral("Create Project"),
                         QStringLiteral("Directory already exists."));
    return;
  }

  std::error_code ec;
  std::filesystem::create_directories(projectDir.toStdString(), ec);
  if (ec) {
    QMessageBox::critical(this, QStringLiteral("Create Project"),
                          QStringLiteral("Failed to create directory."));
    return;
  }

  // create engine.toml
  std::string tomlContent = "[window]\n";
  tomlContent += "title = \"" + name.toStdString() + "\"\n";
  tomlContent += "width = 1280\n";
  tomlContent += "height = 720\n";
  tomlContent += "resizable = true\n";
  tomlContent += "fullscreen = false\n";
  tomlContent += "vsync = true\n\n";
  tomlContent += "[render]\n";
  tomlContent += "backend = \"vulkan\"\n";
  tomlContent += "validation = true\n";
  tomlContent += "antialiasing = \"none\"\n";

  auto tomlPath =
      (std::filesystem::path(projectDir.toStdString()) / "engine.toml")
          .string();
  std::ofstream tomlFile(tomlPath);
  if (tomlFile.is_open()) {
    tomlFile << tomlContent;
  }

  // create config directory
  std::filesystem::create_directories(
      std::filesystem::path(projectDir.toStdString()) / "config", ec);

  createdPath_ = projectDir.toStdString();
  s_logger.info("Created new project '{}' at '{}'", name.toStdString(),
                createdPath_);
  accept();
}

void NewProjectDialog::validate() {
  auto *buttons = findChild<QDialogButtonBox *>();
  if (buttons != nullptr) {
    auto *okBtn = buttons->button(QDialogButtonBox::Ok);
    if (okBtn != nullptr) {
      okBtn->setEnabled(!nameEdit_->text().trimmed().isEmpty());
    }
  }
}

} // namespace RaidenEditor
