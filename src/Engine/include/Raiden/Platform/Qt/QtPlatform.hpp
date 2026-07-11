#pragma once

#include <Raiden/Platform/IPlatform.hpp>
#include <Raiden/Platform/InputState.hpp>

#include <QWindow>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

struct wl_display;
struct xcb_connection_t;

namespace Raiden::Platform {

class EngineWindow : public QWindow {
  Q_OBJECT
public:
  explicit EngineWindow(InputState &inputState): inputState_(inputState) {}
  ~EngineWindow() override = default;

  EngineWindow(const EngineWindow &) = delete;
  EngineWindow &operator=(const EngineWindow &) = delete;
  EngineWindow(EngineWindow &&) = delete;
  EngineWindow &operator=(EngineWindow &&) = delete;

  [[nodiscard]] bool shouldClose() const { return closeRequested_; }
  void resetCloseFlag() { closeRequested_ = false; }

  [[nodiscard]] bool wasResized() const { return resized_; }
  void clearResizeFlag() { resized_ = false; }

protected:
  bool event(QEvent *event) override;

private:
  InputState &inputState_;
  bool closeRequested_ = false;
  bool resized_ = false;

  static int qtKeyToScanCode(int qtKey);
};

class QtPlatform : public IPlatform {
public:
  QtPlatform() = default;
  ~QtPlatform() override;

  QtPlatform(const QtPlatform &) = delete;
  QtPlatform &operator=(const QtPlatform &) = delete;
  QtPlatform(QtPlatform &&) = delete;
  QtPlatform &operator=(QtPlatform &&) = delete;

  bool init(const ::Raiden::Core::WindowConfig &config,
            ::Raiden::Core::RenderBackend backend) override;
  void shutdown() override;
  bool pollEvents() override;

  void *getNativeWindowHandle() override;
  void getWindowSize(int &width, int &height) const override;
  bool hasResizePending() override;
  bool isWindowExposed() override;
  void flushPendingPresentation() override;
  void setRelativeMouseMode(bool enabled) override;
  void endInputFrame() override;

  [[nodiscard]] const InputState &getInputState() const override { return inputState_; }

  [[nodiscard]] std::vector<const char *> getRequiredInstanceExtensions() const override;
  bool createVulkanSurface(VkInstance instance, VkSurfaceKHR *surface) override;

  void setVulkanWindow(QWindow *window) { window_ = window; }
  [[nodiscard]] QWindow *vulkanWindow() const { return window_; }

  [[nodiscard]] InputState &inputState() { return inputState_; }

private:
  QWindow *window_ = nullptr;
  InputState inputState_{};
  bool running_ = true;
  xcb_connection_t *xcbConnection_ = nullptr;
  wl_display *wlDisplay_ = nullptr;
};

} // namespace Raiden::Platform
