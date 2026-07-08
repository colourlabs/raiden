#include <RaidenEditor/Helpers/FramelessResizeHelper.hpp>

#include <QEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QWindow>

namespace RaidenEditor {

FramelessResizeHelper::FramelessResizeHelper(QMainWindow *window)
    : QObject(window), window_(window) {
  window_->setMouseTracking(true);
}

bool FramelessResizeHelper::eventFilter(QObject *watched, QEvent *event) {
  if (watched != window_) {
    return false;
  }

  if (window_->isMaximized()) {
    return false;
  }

  const QRect r = window_->rect();

  if (event->type() == QEvent::MouseMove) {
    auto *me = static_cast<QMouseEvent *>(event);
    const QPoint p = me->pos();

    const bool left = p.x() <= kBorderWidth;
    const bool right = p.x() >= r.width() - kBorderWidth;
    const bool top = p.y() <= kBorderWidth;
    const bool bottom = p.y() >= r.height() - kBorderWidth;

    if ((left && top) || (right && bottom)) {
      window_->setCursor(Qt::SizeFDiagCursor);
    } else if ((right && top) || (left && bottom)) {
      window_->setCursor(Qt::SizeBDiagCursor);
    } else if (left || right) {
      window_->setCursor(Qt::SizeHorCursor);
    } else if (top || bottom) {
      window_->setCursor(Qt::SizeVerCursor);
    } else {
      window_->unsetCursor();
    }

    return false;
  }

  if (event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    if (me->button() != Qt::LeftButton) {
      return false;
    }

    const QPoint p = me->pos();
    const bool left = p.x() <= kBorderWidth;
    const bool right = p.x() >= r.width() - kBorderWidth;
    const bool top = p.y() <= kBorderWidth;
    const bool bottom = p.y() >= r.height() - kBorderWidth;

    Qt::Edges edges;
    if (top) {
      edges |= Qt::TopEdge;
    }
    if (bottom) {
      edges |= Qt::BottomEdge;
    }
    if (left) {
      edges |= Qt::LeftEdge;
    }
    if (right) {
      edges |= Qt::RightEdge;
    }

    if (edges != Qt::Edges() && (window_->windowHandle() != nullptr)) {
      window_->windowHandle()->startSystemResize(edges);
      return true;
    }
  }

  return false;
}

} // namespace RaidenEditor