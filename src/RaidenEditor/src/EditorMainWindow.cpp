#include <RaidenEditor/CustomWidgets/CustomTitleBar.hpp>
#include <RaidenEditor/EditorMainWindow.hpp>
#include <RaidenEditor/Helpers/FramelessResizeHelper.hpp>
#include <RaidenEditor/Icons/FluentIcons.hpp>

#include <Raiden/Platform/Qt/QtPlatform.hpp>

#include <QApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QDebug>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

namespace RaidenEditor {

EditorMainWindow::EditorMainWindow(Raiden::Platform::QtPlatform &platform)
    : mainWindow_(new QMainWindow()) {

  mainWindow_->setWindowFlag(Qt::FramelessWindowHint);
  mainWindow_->setAttribute(Qt::WA_TranslucentBackground, false);

  // vulkan setup
  auto &inputState = platform.inputState();
  auto *engineWindow = new Raiden::Platform::EngineWindow(inputState);
  engineWindow->setSurfaceType(QWindow::VulkanSurface);
  vkWindow_ = engineWindow;
  platform.setVulkanWindow(vkWindow_);

  QWidget *container = QWidget::createWindowContainer(vkWindow_);
  container->setFocusPolicy(Qt::StrongFocus);
  container->setMinimumSize(320, 240);
  mainWindow_->setCentralWidget(container);
  viewportContainer_ = container;

  createTitleBar();
  createViewportToolBar();
  createSceneDock();
  createInspectorDock();
  createWorldSettingsDock();
  createAssetDock();
  createOutputDock();
  createMenuBar();
  createStatusBar();

  mainWindow_->setDockOptions(QMainWindow::AllowTabbedDocks |
                              QMainWindow::GroupedDragging);
  mainWindow_->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
  mainWindow_->tabifyDockWidget(inspectorDock_, worldSettingsDock_);
  inspectorDock_->raise();

  mainWindow_->tabifyDockWidget(assetDock_, outputDock_);
  assetDock_->raise();

  sceneDock_->setMinimumWidth(190);
  inspectorDock_->setMinimumWidth(230);
  worldSettingsDock_->setMinimumWidth(230);
  assetDock_->setMinimumWidth(260);
  outputDock_->setMinimumWidth(260);
  assetDock_->setMinimumHeight(160);
  outputDock_->setMinimumHeight(160);

  auto forceViewportRepaint = [this] {
    if (vkWindow_ != nullptr) {
      vkWindow_->requestUpdate();
    }
  };

  QObject::connect(sceneDock_, &QDockWidget::dockLocationChanged, mainWindow_,
                   forceViewportRepaint);
  QObject::connect(inspectorDock_, &QDockWidget::dockLocationChanged,
                   mainWindow_, forceViewportRepaint);
  QObject::connect(worldSettingsDock_, &QDockWidget::dockLocationChanged,
                   mainWindow_, forceViewportRepaint);
  QObject::connect(assetDock_, &QDockWidget::dockLocationChanged, mainWindow_,
                   forceViewportRepaint);
  QObject::connect(outputDock_, &QDockWidget::dockLocationChanged, mainWindow_,
                   forceViewportRepaint);

  QObject::connect(sceneDock_, &QDockWidget::visibilityChanged, mainWindow_,
                   forceViewportRepaint);
  QObject::connect(inspectorDock_, &QDockWidget::visibilityChanged, mainWindow_,
                   forceViewportRepaint);
  QObject::connect(worldSettingsDock_, &QDockWidget::visibilityChanged,
                   mainWindow_, forceViewportRepaint);
  QObject::connect(assetDock_, &QDockWidget::visibilityChanged, mainWindow_,
                   forceViewportRepaint);
  QObject::connect(outputDock_, &QDockWidget::visibilityChanged, mainWindow_,
                   forceViewportRepaint);

  auto suppressDuringDrag = [this] {
    if (viewportContainer_ == nullptr) {
      return;
    }

    viewportContainer_->setUpdatesEnabled(false);

    QTimer::singleShot(0, mainWindow_, [this] {
      if (viewportContainer_ != nullptr) {
        viewportContainer_->setUpdatesEnabled(true);
      }
      if (vkWindow_ != nullptr) {
        vkWindow_->requestUpdate();
      }

      mainWindow_->layout()->activate();
    });
  };

  QObject::connect(sceneDock_, &QDockWidget::topLevelChanged, mainWindow_,
                   suppressDuringDrag);
  QObject::connect(inspectorDock_, &QDockWidget::topLevelChanged, mainWindow_,
                   suppressDuringDrag);
  QObject::connect(worldSettingsDock_, &QDockWidget::topLevelChanged,
                   mainWindow_, suppressDuringDrag);
  QObject::connect(assetDock_, &QDockWidget::topLevelChanged, mainWindow_,
                   suppressDuringDrag);
  QObject::connect(outputDock_, &QDockWidget::topLevelChanged, mainWindow_,
                   suppressDuringDrag);

  mainWindow_->installEventFilter(new FramelessResizeHelper(mainWindow_));

  mainWindow_->resize(1280, 720);

  QTimer::singleShot(0, mainWindow_, [this] {
    mainWindow_->layout()->activate();
    QApplication::processEvents();
    const QByteArray state = mainWindow_->saveState();
    mainWindow_->restoreState(state);
  });
}

EditorMainWindow::~EditorMainWindow() {
  delete vkWindow_;
  delete mainWindow_;
}

void EditorMainWindow::show() { mainWindow_->show(); }

void EditorMainWindow::createTitleBar() {
  titleBar_ = new CustomTitleBar(mainWindow_);
  titleBar_->setTitleText(QStringLiteral("Raiden Editor"));
  mainWindow_->setMenuWidget(titleBar_);
}

void EditorMainWindow::createMenuBar() {
  menuBar_ = titleBar_->menuBar();

  auto *fileMenu = menuBar_->addMenu(QStringLiteral("&File"));
  fileMenu->addAction(QStringLiteral("New Scene"));
  fileMenu->addAction(QStringLiteral("Open Scene..."));
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("Save"));
  fileMenu->addAction(QStringLiteral("Save As..."));
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("Exit"), mainWindow_, &QWidget::close);

  auto *editMenu = menuBar_->addMenu(QStringLiteral("&Edit"));
  editMenu->addAction(QStringLiteral("Undo"));
  editMenu->addAction(QStringLiteral("Redo"));
  editMenu->addSeparator();
  editMenu->addAction(QStringLiteral("Preferences..."));

  auto *viewMenu = menuBar_->addMenu(QStringLiteral("&View"));
  viewMenu->addAction(sceneDock_->toggleViewAction());
  viewMenu->addAction(inspectorDock_->toggleViewAction());
  viewMenu->addAction(worldSettingsDock_->toggleViewAction());
  viewMenu->addAction(assetDock_->toggleViewAction());
  viewMenu->addAction(outputDock_->toggleViewAction());

  auto *helpMenu = menuBar_->addMenu(QStringLiteral("&Help"));
  helpMenu->addAction(QStringLiteral("About Raiden Editor"));
}

void EditorMainWindow::createViewportToolBar() {
  viewportToolBar_ = new QToolBar(QStringLiteral("Playback"), mainWindow_);
  viewportToolBar_->setObjectName(QStringLiteral("ViewportToolBar"));
  viewportToolBar_->setMovable(false);
  viewportToolBar_->setIconSize(QSize(18, 18));

  auto addToolButton = [&](const QIcon &icon, const QString &tip) {
    auto *btn = new QToolButton(viewportToolBar_);
    btn->setIcon(icon);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    viewportToolBar_->addWidget(btn);
    return btn;
  };

  playButton_ = addToolButton(FluentIcon::load("Play"), QStringLiteral("Play"));
  pauseButton_ =
      addToolButton(FluentIcon::load("Pause"), QStringLiteral("Pause"));
  stopButton_ = addToolButton(FluentIcon::load("Stop"), QStringLiteral("Stop"));
  viewportToolBar_->addSeparator();
  addToolButton(FluentIcon::load("Wrench"), QStringLiteral("Build"));

  mainWindow_->addToolBar(Qt::TopToolBarArea, viewportToolBar_);
}

// scene hierarchy dock (Roblox "Explorer" / Unreal "World Outliner")
void EditorMainWindow::createSceneDock() {
  sceneDock_ = new QDockWidget(QStringLiteral("Scene Hierarchy"), mainWindow_);
  sceneDock_->setObjectName(QStringLiteral("SceneHierarchyDock"));
  sceneDock_->setFeatures(QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetFloatable);

  auto *tree = new QTreeWidget();
  tree->setHeaderHidden(true);
  tree->setRootIsDecorated(true);
  tree->setAnimated(true);
  tree->setIndentation(16);

  auto *player = new QTreeWidgetItem(tree);
  player->setText(0, QStringLiteral("Player"));
  player->setSelected(true);

  auto *camera = new QTreeWidgetItem(tree);
  camera->setText(0, QStringLiteral("Camera"));

  auto *light = new QTreeWidgetItem(tree);
  light->setText(0, QStringLiteral("Light"));

  auto *ground = new QTreeWidgetItem(tree);
  ground->setText(0, QStringLiteral("Ground"));

  auto *enemy = new QTreeWidgetItem(tree);
  enemy->setText(0, QStringLiteral("Enemy"));

  auto *props = new QTreeWidgetItem(tree);
  props->setText(0, QStringLiteral("Props"));

  sceneTree_ = tree;
  sceneDock_->setWidget(tree);
  mainWindow_->addDockWidget(Qt::LeftDockWidgetArea, sceneDock_);
}

// inspector dock ("Properties" / "Details")
void EditorMainWindow::createInspectorDock() {
  inspectorDock_ = new QDockWidget(QStringLiteral("Inspector"), mainWindow_);
  inspectorDock_->setObjectName(QStringLiteral("InspectorDock"));
  inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetFloatable);

  // custom title bar with Add Component button
  auto *titleWidget = new QWidget();
  auto *titleLayout = new QHBoxLayout(titleWidget);
  titleLayout->setContentsMargins(8, 0, 4, 0);
  titleLayout->setSpacing(0);

  auto *titleLabel = new QLabel(QStringLiteral("Inspector"));
  titleLayout->addWidget(titleLabel, 1);

  auto *menuButton = new QPushButton();
  menuButton->setIcon(FluentIcon::load("MoreHorizontal"));
  menuButton->setIconSize(QSize(16, 16));
  menuButton->setObjectName(QStringLiteral("ComponentMenuButton"));
  menuButton->setFixedSize(28, 28);
  menuButton->setFlat(true);
  titleLayout->addWidget(menuButton);

  inspectorDock_->setTitleBarWidget(titleWidget);

  auto *scrollContent = new QWidget();
  auto *form = new QFormLayout(scrollContent);
  form->setContentsMargins(8, 8, 8, 8);
  form->setSpacing(4);

  auto *transformGroup = new QGroupBox(QStringLiteral("Transform"));
  auto *transformLayout = new QFormLayout(transformGroup);

  auto addVec3Row = [&](const QString &label, double x, double y, double z) {
    auto *row = new QWidget();
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(4);

    auto *sbX = new QDoubleSpinBox();
    sbX->setRange(-99999.0, 99999.0);
    sbX->setValue(x);
    sbX->setSingleStep(0.1);
    sbX->setPrefix(QStringLiteral("X: "));

    auto *sbY = new QDoubleSpinBox();
    sbY->setRange(-99999.0, 99999.0);
    sbY->setValue(y);
    sbY->setSingleStep(0.1);
    sbY->setPrefix(QStringLiteral("Y: "));

    auto *sbZ = new QDoubleSpinBox();
    sbZ->setRange(-99999.0, 99999.0);
    sbZ->setValue(z);
    sbZ->setSingleStep(0.1);
    sbZ->setPrefix(QStringLiteral("Z: "));

    hl->addWidget(sbX);
    hl->addWidget(sbY);
    hl->addWidget(sbZ);
    transformLayout->addRow(label, row);
  };

  addVec3Row(QStringLiteral("Position"), 0.0, 1.0, 0.0);
  addVec3Row(QStringLiteral("Rotation"), 0.0, 45.0, 0.0);
  addVec3Row(QStringLiteral("Scale"), 1.0, 1.0, 1.0);

  form->addRow(transformGroup);

  auto *meshGroup = new QGroupBox(QStringLiteral("Mesh Renderer"));
  auto *meshLayout = new QFormLayout(meshGroup);
  auto *meshLabel = new QLabel(QStringLiteral("Mesh_Player.fbx"));
  meshLabel->setWordWrap(true);
  meshLayout->addRow(QStringLiteral("Mesh"), meshLabel);

  auto *matLabel = new QLabel(QStringLiteral("PBR_Default"));
  matLabel->setWordWrap(true);
  meshLayout->addRow(QStringLiteral("Material"), matLabel);
  form->addRow(meshGroup);

  inspectorDock_->setWidget(scrollContent);
  mainWindow_->addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

// Unreal-style "World Settings" tab, tabified alongside the Inspector
void EditorMainWindow::createWorldSettingsDock() {
  worldSettingsDock_ =
      new QDockWidget(QStringLiteral("World Settings"), mainWindow_);
  worldSettingsDock_->setObjectName(QStringLiteral("WorldSettingsDock"));
  worldSettingsDock_->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable |
                                  QDockWidget::DockWidgetFloatable);

  auto *content = new QWidget();
  auto *form = new QFormLayout(content);
  form->setContentsMargins(8, 8, 8, 8);
  form->setSpacing(6);

  auto *worldGroup = new QGroupBox(QStringLiteral("Environment"));
  auto *worldLayout = new QFormLayout(worldGroup);

  auto *gravitySpin = new QDoubleSpinBox();
  gravitySpin->setRange(-100.0, 0.0);
  gravitySpin->setValue(-9.81);
  worldLayout->addRow(QStringLiteral("Gravity"), gravitySpin);

  auto *skyLabel = new QLabel(QStringLiteral("Sky_Default"));
  worldLayout->addRow(QStringLiteral("Skybox"), skyLabel);

  form->addRow(worldGroup);
  worldSettingsDock_->setWidget(content);
}

// asset browser dock (blends Unreal's Content Drawer breadcrumb/search
// with Roblox Studio's simple grid)
void EditorMainWindow::createAssetDock() {
  assetDock_ = new QDockWidget(QStringLiteral("Asset Browser"), mainWindow_);
  assetDock_->setObjectName(QStringLiteral("AssetBrowserDock"));
  assetDock_->setFeatures(QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetFloatable);

  auto *container = new QWidget();
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->setSpacing(4);

  auto *toolbarRow = new QWidget();
  auto *toolbarLayout = new QHBoxLayout(toolbarRow);
  toolbarLayout->setContentsMargins(0, 0, 0, 0);
  toolbarLayout->setSpacing(6);

  auto *breadcrumb = new QLabel(QStringLiteral("Content  /  Game"));
  breadcrumb->setObjectName(QStringLiteral("Breadcrumb"));

  auto *searchBox = new QLineEdit();
  searchBox->setPlaceholderText(QStringLiteral("Search assets..."));
  searchBox->setClearButtonEnabled(true);
  searchBox->setMaximumWidth(220);

  toolbarLayout->addWidget(breadcrumb);
  toolbarLayout->addStretch(1);
  toolbarLayout->addWidget(searchBox);
  layout->addWidget(toolbarRow);

  auto *list = new QListWidget();
  list->setViewMode(QListView::IconMode);
  list->setIconSize(QSize(48, 48));
  list->setGridSize(QSize(84, 84));
  list->setResizeMode(QListView::Adjust);
  list->setWordWrap(true);
  list->setSpacing(6);

  list->addItem(QStringLiteral("Meshes/"));
  list->addItem(QStringLiteral("Textures/"));
  list->addItem(QStringLiteral("Materials/"));
  list->addItem(QStringLiteral("Shaders/"));
  list->addItem(QStringLiteral("Animations/"));
  list->addItem(QStringLiteral("Audio/"));

  assetList_ = list;
  layout->addWidget(list, 1);

  assetDock_->setWidget(container);
  mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, assetDock_);
}

// Roblox Studio-style bottom "Output" log, tabified with the Asset Browser
void EditorMainWindow::createOutputDock() {
  outputDock_ = new QDockWidget(QStringLiteral("Output"), mainWindow_);
  outputDock_->setObjectName(QStringLiteral("OutputDock"));
  outputDock_->setFeatures(QDockWidget::DockWidgetMovable |
                           QDockWidget::DockWidgetClosable |
                           QDockWidget::DockWidgetFloatable);

  auto *log = new QPlainTextEdit();
  log->setReadOnly(true);
  log->setPlaceholderText(QStringLiteral("Editor log output..."));
  log->appendPlainText(QStringLiteral("[Info] Editor initialized."));
  log->appendPlainText(QStringLiteral("[Info] Datapack mounted."));

  outputLog_ = log;
  outputDock_->setWidget(log);
}

// status bar
void EditorMainWindow::createStatusBar() {
  statusBar_ = mainWindow_->statusBar();
  statusBar_->showMessage(QStringLiteral("Ready"));
}

} // namespace RaidenEditor