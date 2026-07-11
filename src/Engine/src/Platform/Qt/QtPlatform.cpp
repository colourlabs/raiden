#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XCB_KHR

#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xcb/xcb.h>

#include <Raiden/Platform/Qt/QtPlatform.hpp>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <iostream>

struct XcbSurfaceCreateInfoKHR {
  VkStructureType sType;
  const void *pNext;
  VkFlags flags;
  xcb_connection_t *connection;
  uint32_t window;
};

namespace Raiden::Platform {

int EngineWindow::qtKeyToScanCode(int qtKey) {
  if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
    return 4 + (qtKey - Qt::Key_A);
  }

  if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
    if (qtKey == Qt::Key_0) {
      return 39;
    }
    return 30 + (qtKey - Qt::Key_1);
  }

  switch (qtKey) {
  case Qt::Key_Return:
    return 40;
  case Qt::Key_Escape:
    return 41;
  case Qt::Key_Backspace:
    return 42;
  case Qt::Key_Tab:
    return 43;
  case Qt::Key_Space:
    return 44;
  case Qt::Key_Minus:
    return 45;
  case Qt::Key_Equal:
    return 46;
  case Qt::Key_BracketLeft:
    return 47;
  case Qt::Key_BracketRight:
    return 48;
  case Qt::Key_Semicolon:
    return 51;
  case Qt::Key_Apostrophe:
    return 52;
  case Qt::Key_Comma:
    return 54;
  case Qt::Key_Period:
    return 55;
  case Qt::Key_Slash:
    return 56;
  case Qt::Key_F1:
    return 58;
  case Qt::Key_F2:
    return 59;
  case Qt::Key_F3:
    return 60;
  case Qt::Key_F4:
    return 61;
  case Qt::Key_F5:
    return 62;
  case Qt::Key_F6:
    return 63;
  case Qt::Key_F7:
    return 64;
  case Qt::Key_F8:
    return 65;
  case Qt::Key_F9:
    return 66;
  case Qt::Key_F10:
    return 67;
  case Qt::Key_F11:
    return 68;
  case Qt::Key_F12:
    return 69;
  case Qt::Key_Delete:
    return 76;
  case Qt::Key_Home:
    return 74;
  case Qt::Key_End:
    return 77;
  case Qt::Key_Right:
    return 79;
  case Qt::Key_Left:
    return 80;
  case Qt::Key_Down:
    return 81;
  case Qt::Key_Up:
    return 82;
  case Qt::Key_Shift:
    return 225;
  case Qt::Key_Control:
    return 224;
  case Qt::Key_Alt:
    return 226;
  default:
    return -1;
  }
}

bool EngineWindow::event(QEvent *event) {
  switch (event->type()) {
  case QEvent::KeyPress:
  case QEvent::KeyRelease: {
    auto *ke = static_cast<QKeyEvent *>(event);
    int idx = qtKeyToScanCode(ke->key());
    if (idx >= 0 && idx < InputState::kMaxKeys) {
      inputState_.keysDown[idx] = (event->type() == QEvent::KeyPress);
    }
    return true;
  }

  case QEvent::MouseMove: {
    auto *me = static_cast<QMouseEvent *>(event);
    int newX = static_cast<int>(me->localPos().x());
    int newY = static_cast<int>(me->localPos().y());
    inputState_.mouseDeltaX += newX - inputState_.mouseX;
    inputState_.mouseDeltaY += newY - inputState_.mouseY;
    inputState_.mouseX = newX;
    inputState_.mouseY = newY;
    return true;
  }

  case QEvent::MouseButtonPress:
  case QEvent::MouseButtonRelease: {
    auto *me = static_cast<QMouseEvent *>(event);
    int idx = -1;
    switch (me->button()) {
    case Qt::LeftButton:
      idx = 0;
      break;
    case Qt::RightButton:
      idx = 1;
      break;
    case Qt::MiddleButton:
      idx = 2;
      break;
    default:
      break;
    }
    if (idx >= 0) {
      inputState_.mouseButtons[idx] =
          (event->type() == QEvent::MouseButtonPress);
    }
    return true;
  }

  case QEvent::Wheel: {
    auto *we = static_cast<QWheelEvent *>(event);
    inputState_.scrollY = static_cast<float>(we->angleDelta().y()) / 120.0F;
    return true;
  }

  case QEvent::Close:
    closeRequested_ = true;
    return true;

  case QEvent::Resize:
    resized_ = true;
    return QWindow::event(event);

  default:
    break;
  }
  return QWindow::event(event);
}

QtPlatform::~QtPlatform() { shutdown(); }

bool QtPlatform::init(const ::Raiden::Core::WindowConfig & /*config*/,
                      ::Raiden::Core::RenderBackend backend) {
  (void)backend;
  return true;
}

void QtPlatform::shutdown() {
  if (xcbConnection_ != nullptr) {
    xcb_disconnect(xcbConnection_);
    xcbConnection_ = nullptr;
  }
  wlDisplay_ = nullptr;
  window_ = nullptr;
  running_ = false;
}

bool QtPlatform::pollEvents() {
  QCoreApplication::processEvents();

  if (window_ != nullptr) {
    auto *ew = qobject_cast<EngineWindow *>(window_);
    if (ew != nullptr && ew->shouldClose()) {
      running_ = false;
    }
  }

  return running_;
}

void *QtPlatform::getNativeWindowHandle() {
  return reinterpret_cast<void *>(window_);
}

void QtPlatform::getWindowSize(int &width, int &height) const {
  if (window_ != nullptr) {
    width = window_->width();
    height = window_->height();
  }
}

bool QtPlatform::hasResizePending() {
  if (window_ != nullptr) {
    auto *ew = qobject_cast<EngineWindow *>(window_);
    if (ew != nullptr && ew->wasResized()) {
      ew->clearResizeFlag();
      return true;
    }
  }
  return false;
}

bool QtPlatform::isWindowExposed() {
  return window_ != nullptr && window_->isExposed();
}

void QtPlatform::flushPendingPresentation() {
  if (wlDisplay_ != nullptr) {
    wl_display_roundtrip(wlDisplay_);
  }
}

void QtPlatform::setRelativeMouseMode(bool enabled) {
  if (window_ == nullptr) {
    return;
  }
  if (enabled) {
    window_->setCursor(Qt::BlankCursor);
    window_->setMouseGrabEnabled(true);
  } else {
    window_->setCursor(Qt::ArrowCursor);
    window_->setMouseGrabEnabled(false);
  }
}

void QtPlatform::endInputFrame() { inputState_.endFrame(); }

std::vector<const char *> QtPlatform::getRequiredInstanceExtensions() const {
  auto name = qGuiApp->platformName();
  if (name == "wayland") {
    return {"VK_KHR_surface", "VK_KHR_wayland_surface"};
  }
  return {"VK_KHR_surface", "VK_KHR_xcb_surface"};
}

bool QtPlatform::createVulkanSurface(VkInstance instance,
                                     VkSurfaceKHR *surface) {
  if (window_ == nullptr) {
    return false;
  }

  auto name = qGuiApp->platformName();

  if (name == "wayland") {
    auto *native = qGuiApp->platformNativeInterface();
    auto *wlSurface = static_cast<wl_surface *>(
        native->nativeResourceForWindow(QByteArrayLiteral("surface"), window_));
    if (wlSurface == nullptr) {
      std::cerr << "QtPlatform: failed to get wl_surface from Qt\n";
      return false;
    }

    wlDisplay_ = static_cast<wl_display *>(
        native->nativeResourceForWindow(QByteArrayLiteral("display"), window_));
    if (wlDisplay_ == nullptr) {
      std::cerr << "QtPlatform: failed to get wl_display from Qt\n";
      return false;
    }

    auto func = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));
    if (func == nullptr) {
      std::cerr << "QtPlatform: vkCreateWaylandSurfaceKHR not found\n";
      return false;
    }

    VkWaylandSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    createInfo.display = wlDisplay_;
    createInfo.surface = wlSurface;

    VkResult res = func(instance, &createInfo, nullptr, surface);
    if (res != VK_SUCCESS) {
      std::cerr << "QtPlatform: vkCreateWaylandSurfaceKHR failed: " << res
                << "\n";
      return false;
    }

    return true;
  }

  // XCB path
  int screen = 0;
  xcbConnection_ = xcb_connect(nullptr, &screen);
  if (xcb_connection_has_error(xcbConnection_)) {
    std::cerr << "QtPlatform: failed to connect to X server via XCB\n";
    return false;
  }

  auto func = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
      vkGetInstanceProcAddr(instance, "vkCreateXcbSurfaceKHR"));
  if (func == nullptr) {
    std::cerr << "QtPlatform: vkCreateXcbSurfaceKHR not found\n";
    return false;
  }

  VkXcbSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  createInfo.connection = xcbConnection_;
  createInfo.window = static_cast<xcb_window_t>(window_->winId());

  VkResult res = func(instance, &createInfo, nullptr, surface);
  if (res != VK_SUCCESS) {
    std::cerr << "QtPlatform: vkCreateXcbSurfaceKHR failed: " << res << "\n";
    return false;
  }

  return true;
}

} // namespace Raiden::Platform
