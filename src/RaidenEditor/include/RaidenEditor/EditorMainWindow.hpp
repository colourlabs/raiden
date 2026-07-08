#pragma once

class QMainWindow;
class QDockWidget;
class QWindow;
class QWidget;
class QTreeWidget;
class QListWidget;
class QDoubleSpinBox;
class QStatusBar;
class QMenuBar;
class QToolBar;
class QToolButton;
class QLineEdit;
class QPlainTextEdit;

namespace Raiden::Platform {

class QtPlatform;

}

namespace RaidenEditor {

class CustomTitleBar;

class EditorMainWindow {
public:
  explicit EditorMainWindow(Raiden::Platform::QtPlatform &platform);
  ~EditorMainWindow();
  EditorMainWindow(const EditorMainWindow &) = delete;
  EditorMainWindow &operator=(const EditorMainWindow &) = delete;
  EditorMainWindow(EditorMainWindow &&) noexcept;
  EditorMainWindow &operator=(EditorMainWindow &&) noexcept;

  void show();
  void setRunning(bool r) { running_ = r; }

  [[nodiscard]] QTreeWidget *sceneTree() const { return sceneTree_; }
  [[nodiscard]] QListWidget *assetList() const { return assetList_; }

private:
  void createTitleBar();
  void createViewportToolBar();
  void createMenuBar();
  void createSceneDock();
  void createInspectorDock();
  void createWorldSettingsDock();
  void createAssetDock();
  void createOutputDock();
  void createStatusBar();

  QMainWindow *mainWindow_ = nullptr;
  QWindow *vkWindow_ = nullptr;
  QWidget *viewportContainer_ = nullptr;
  CustomTitleBar *titleBar_ = nullptr;
  QMenuBar *menuBar_ = nullptr;
  QStatusBar *statusBar_ = nullptr;
  QToolBar *viewportToolBar_ = nullptr;
  QToolButton *playButton_ = nullptr;
  QToolButton *pauseButton_ = nullptr;
  QToolButton *stopButton_ = nullptr;
  QDockWidget *sceneDock_ = nullptr;
  QDockWidget *inspectorDock_ = nullptr;
  QDockWidget *worldSettingsDock_ = nullptr;
  QDockWidget *assetDock_ = nullptr;
  QDockWidget *outputDock_ = nullptr;
  QTreeWidget *sceneTree_ = nullptr;
  QListWidget *assetList_ = nullptr;
  QPlainTextEdit *outputLog_ = nullptr;

  bool running_ = true;
};

} // namespace RaidenEditor