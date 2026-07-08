#pragma once

#include <QObject>

class QMainWindow;

namespace RaidenEditor {

// frameless windows lose their OS resize grips. this installs an event
// filter on the top-level window that detects the mouse hovering/pressing
// near an edge and hands off to the platform's native resize
// (QWindow::startSystemResize), so resizing still feels native.
class FramelessResizeHelper : public QObject {
  Q_OBJECT
public:
  explicit FramelessResizeHelper(QMainWindow *window);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QMainWindow *window_;
  static constexpr int kBorderWidth = 6;
};

} // namespace RaidenEditor