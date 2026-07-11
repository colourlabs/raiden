#pragma once

#include <QDialog>

#include <string>

class QLineEdit;

namespace RaidenEditor {

class NewProjectDialog : public QDialog {
  Q_OBJECT
public:
  explicit NewProjectDialog(std::string gamesDir,
                            QWidget *parent = nullptr);

  [[nodiscard]] std::string createdProjectPath() const {
    return createdPath_;
  }

private slots:
  void onBrowse();
  void onCreate();

private:
  void validate();

  QLineEdit *nameEdit_ = nullptr;
  QLineEdit *locationEdit_ = nullptr;
  std::string gamesDir_;
  std::string createdPath_;
};

} // namespace RaidenEditor
