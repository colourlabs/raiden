#pragma once

#include <QObject>

#include <RaidenEditor/GizmoMode.hpp>

#include <memory>
#include <string>

class QDockWidget;
class QLineEdit;
class QListWidget;
class QMainWindow;
class QMenuBar;
class QPlainTextEdit;
class QStatusBar;
class QTabWidget;
class QTimer;
class QToolBar;
class QToolButton;
class QTreeWidget;
class QWidget;
class QWindow;

namespace Raiden::Core {
class IVirtualFileSystem;
}

namespace Raiden::Platform {

class QtPlatform;

}

namespace RaidenEditor {

class CustomTitleBar;
class EditorContext;

class EditorMainWindow : public QObject {
  Q_OBJECT
public:
  explicit EditorMainWindow(Raiden::Platform::QtPlatform &platform,
                            std::string_view datapackPath,
                            Raiden::Core::IVirtualFileSystem &vfs);
  ~EditorMainWindow() override;
  EditorMainWindow(const EditorMainWindow &) = delete;
  EditorMainWindow &operator=(const EditorMainWindow &) = delete;
  EditorMainWindow(EditorMainWindow &&) = delete;
  EditorMainWindow &operator=(EditorMainWindow &&) = delete;

  void show();
  void setRunning(bool r) { running_ = r; }

  [[nodiscard]] EditorContext *context() const { return context_; }
  [[nodiscard]] QTreeWidget *sceneTree() const { return sceneTree_; }
  [[nodiscard]] QListWidget *assetList() const { return assetList_; }

  void saveScene();
  void saveSceneAs();
  void loadScene();
  void newScene();

private slots:
  void onFrameTick();
  void onViewportTabChanged(int index);

private:
  void createTitleBar();
  void createEditorToolBar();
  void createMenuBar();
  void createSceneDock();
  void createInspectorDock();
  void createAssetDock();
  void createOutputDock();
  void createStatusBar();
  void rebuildSceneTree();

  EditorContext *context_ = nullptr;
  std::unique_ptr<QMainWindow> mainWindow_;
  std::unique_ptr<QWindow> vkWindow_;
  QWidget *viewportContainer_ = nullptr;
  CustomTitleBar *titleBar_ = nullptr;
  QMenuBar *menuBar_ = nullptr;
  QStatusBar *statusBar_ = nullptr;
  QTabWidget *viewportTabs_ = nullptr;
  QToolBar *editorToolBar_ = nullptr;
  QToolButton *playButton_ = nullptr;
  QToolButton *pauseButton_ = nullptr;
  QToolButton *stopButton_ = nullptr;
  QToolButton *moveButton_ = nullptr;
  QToolButton *rotateButton_ = nullptr;
  QToolButton *scaleButton_ = nullptr;
  std::string datapackPath_;
  Raiden::Core::IVirtualFileSystem *vfs_ = nullptr;
  Raiden::Platform::QtPlatform *platform_ = nullptr;
  QTimer *frameTimer_ = nullptr;

  QDockWidget *sceneDock_ = nullptr;
  QDockWidget *inspectorDock_ = nullptr;
  QDockWidget *assetDock_ = nullptr;
  QDockWidget *outputDock_ = nullptr;
  QTreeWidget *sceneTree_ = nullptr;
  QTreeWidget *assetTree_ = nullptr;
  QListWidget *assetList_ = nullptr;
  QPlainTextEdit *outputLog_ = nullptr;

  bool running_ = true;
};

} // namespace RaidenEditor
