#include <RaidenEditor/CustomWidgets/CustomTitleBar.hpp>
#include <RaidenEditor/Icons/FluentIcons.hpp>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>

namespace RaidenEditor {

CustomTitleBar::CustomTitleBar(QMainWindow *window, QWidget *parent)
    : QWidget(parent), window_(window), menuBar_(new QMenuBar(this)) {

  setObjectName(QStringLiteral("CustomTitleBar"));
  setFixedHeight(32);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 2, 0, 2);
  layout->setSpacing(0);

  menuBar_->setObjectName(QStringLiteral("EmbeddedMenuBar"));
  menuBar_->setNativeMenuBar(
      false); // keep it inside the strip on every platform
  layout->addWidget(menuBar_);

  titleLabel_ = new QLabel(QStringLiteral("EditorRuntime"), this);
  titleLabel_->setObjectName(QStringLiteral("TitleLabel"));
  titleLabel_->setAlignment(Qt::AlignCenter);
  layout->addWidget(titleLabel_, 1);

  auto makeButton = [&](const QIcon &icon, const QString &objName) {
    auto *btn = new QPushButton(this);
    btn->setIcon(icon);
    btn->setIconSize(QSize(16, 16));
    btn->setObjectName(objName);
    btn->setFixedSize(46, 28);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::ArrowCursor);
    layout->addWidget(btn);
    return btn;
  };

  minButton_ = makeButton(FluentIcon::load("Subtract"), QStringLiteral("MinButton"));
  maxButton_ = makeButton(FluentIcon::load("Square"), QStringLiteral("MaxButton"));
  closeButton_ = makeButton(FluentIcon::load("Dismiss"), QStringLiteral("CloseButton"));

  connect(minButton_, &QPushButton::clicked, window_,
          &QMainWindow::showMinimized);

  connect(maxButton_, &QPushButton::clicked, this, [this]() {
    if (window_->isMaximized()) {
      window_->showNormal();
    } else {
      window_->showMaximized();
    }
    updateMaximizeGlyph();
  });
  connect(closeButton_, &QPushButton::clicked, window_, &QMainWindow::close);

  window_->installEventFilter(this);
}

void CustomTitleBar::setTitleText(const QString &text) {
  titleLabel_->setText(text);
}

void CustomTitleBar::updateMaximizeGlyph() {
  maxButton_->setIcon(
      window_->isMaximized()
          ? FluentIcon::load("SquareMultiple")
          : FluentIcon::load("Square"));
}

bool CustomTitleBar::eventFilter(QObject *obj, QEvent *event) {
  if (obj == window_ && event->type() == QEvent::WindowStateChange) {
    updateMaximizeGlyph();
  }
  return QWidget::eventFilter(obj, event);
}

void CustomTitleBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && (window_->windowHandle() != nullptr)) {
    window_->windowHandle()->startSystemMove();
    event->accept();
  }
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (window_->isMaximized()) {
      window_->showNormal();
    } else {
      window_->showMaximized();
}
    updateMaximizeGlyph();
    event->accept();
  }
}

} // namespace RaidenEditor