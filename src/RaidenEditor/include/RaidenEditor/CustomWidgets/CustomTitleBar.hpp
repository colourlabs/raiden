#pragma once

#include <QWidget>

class QMainWindow;
class QMenuBar;
class QLabel;
class QPushButton;
class QMouseEvent;

namespace RaidenEditor {

class CustomTitleBar : public QWidget {
  Q_OBJECT
public:
  explicit CustomTitleBar(QMainWindow *window, QWidget *parent = nullptr);

  [[nodiscard]] QMenuBar *menuBar() const { return menuBar_; }
  void setTitleText(const QString &text);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void updateMaximizeGlyph();

  QMainWindow *window_ = nullptr;
  QMenuBar *menuBar_ = nullptr;
  QLabel *titleLabel_ = nullptr;
  QPushButton *minButton_ = nullptr;
  QPushButton *maxButton_ = nullptr;
  QPushButton *closeButton_ = nullptr;
};

} // namespace RaidenEditor