#include <RaidenEditor/Core/EditorContext.hpp>
#include <RaidenEditor/CustomWidgets/CustomTitleBar.hpp>
#include <RaidenEditor/EditorMainWindow.hpp>
#include <RaidenEditor/Helpers/FramelessResizeHelper.hpp>
#include <RaidenEditor/Icons/FluentIcons.hpp>

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/Transform.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Engine/ImGuiOverlay.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Platform/Qt/QtPlatform.hpp>
#include <Raiden/Scene/SceneSerializer.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

namespace {

class CloseForwarder : public QObject {
  QWindow *target_;

public:
  CloseForwarder(QMainWindow *source, QWindow *target)
      : QObject(source), target_(target) {
    source->installEventFilter(this);
  }
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::Close && (target_ != nullptr)) {
      target_->close();
    }
    return QObject::eventFilter(obj, event);
  }
};

} // namespace

namespace RaidenEditor {

EditorMainWindow::EditorMainWindow(Raiden::Platform::QtPlatform &platform,
                                   std::string_view datapackPath,
                                   Raiden::Core::IVirtualFileSystem &vfs)
    : QObject(nullptr), context_(new EditorContext(this)),
      mainWindow_(std::make_unique<QMainWindow>()),
      viewportTabs_(new QTabWidget(mainWindow_.get())),
      datapackPath_(datapackPath),
      vfs_(&vfs), platform_(&platform), frameTimer_(new QTimer(this)) {

  mainWindow_->setWindowFlag(Qt::FramelessWindowHint);
  mainWindow_->setAttribute(Qt::WA_TranslucentBackground, false);

  // vulkan setup
  auto &inputState = platform.inputState();
  auto engineWindow =
      std::make_unique<Raiden::Platform::EngineWindow>(inputState);
  engineWindow->setSurfaceType(QWindow::VulkanSurface);
  vkWindow_ = std::move(engineWindow);
  platform.setVulkanWindow(vkWindow_.get());

  QWidget *container = QWidget::createWindowContainer(vkWindow_.get());
  container->setFocusPolicy(Qt::StrongFocus);
  container->setMinimumSize(320, 240);
  viewportContainer_ = container;

  viewportTabs_->setDocumentMode(true);
  viewportTabs_->addTab(container, QStringLiteral("Scene"));
  viewportTabs_->addTab(new QWidget(), QStringLiteral("Game"));
  mainWindow_->setCentralWidget(viewportTabs_);

  QObject::connect(viewportTabs_, &QTabWidget::currentChanged, this,
                   &EditorMainWindow::onViewportTabChanged);
  
  frameTimer_->setInterval(16); // ~60fps
  QObject::connect(frameTimer_, &QTimer::timeout, this,
                   &EditorMainWindow::onFrameTick);
  frameTimer_->start();

  new CloseForwarder(mainWindow_.get(), vkWindow_.get());

  createTitleBar();
  createEditorToolBar();
  createSceneDock();
  createInspectorDock();
  createAssetDock();
  createOutputDock();
  createMenuBar();
  createStatusBar();

  mainWindow_->setDockOptions(QMainWindow::AllowTabbedDocks |
                              QMainWindow::GroupedDragging);
  mainWindow_->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

  mainWindow_->tabifyDockWidget(assetDock_, outputDock_);
  assetDock_->raise();

  sceneDock_->setMinimumWidth(190);
  inspectorDock_->setMinimumWidth(280);
  assetDock_->setMinimumWidth(260);
  outputDock_->setMinimumWidth(260);
  assetDock_->setMinimumHeight(160);
  outputDock_->setMinimumHeight(160);

  auto forceViewportRepaint = [this] {
    if (vkWindow_ != nullptr) {
      vkWindow_->requestUpdate();
    }
  };

  QObject::connect(sceneDock_, &QDockWidget::dockLocationChanged, this,
                   forceViewportRepaint);
  QObject::connect(inspectorDock_, &QDockWidget::dockLocationChanged, this,
                   forceViewportRepaint);
  QObject::connect(assetDock_, &QDockWidget::dockLocationChanged, this,
                   forceViewportRepaint);
  QObject::connect(outputDock_, &QDockWidget::dockLocationChanged, this,
                   forceViewportRepaint);

  QObject::connect(sceneDock_, &QDockWidget::visibilityChanged, this,
                   forceViewportRepaint);
  QObject::connect(inspectorDock_, &QDockWidget::visibilityChanged, this,
                   forceViewportRepaint);
  QObject::connect(assetDock_, &QDockWidget::visibilityChanged, this,
                   forceViewportRepaint);
  QObject::connect(outputDock_, &QDockWidget::visibilityChanged, this,
                   forceViewportRepaint);

  auto suppressDuringDrag = [this] {
    if (viewportContainer_ == nullptr) {
      return;
    }

    viewportContainer_->setUpdatesEnabled(false);

    QTimer::singleShot(0, this, [this] {
      if (viewportContainer_ != nullptr) {
        viewportContainer_->setUpdatesEnabled(true);
      }
      if (vkWindow_ != nullptr) {
        vkWindow_->requestUpdate();
      }

      mainWindow_->layout()->activate();
    });
  };

  QObject::connect(sceneDock_, &QDockWidget::topLevelChanged, this,
                   suppressDuringDrag);
  QObject::connect(inspectorDock_, &QDockWidget::topLevelChanged, this,
                   suppressDuringDrag);
  QObject::connect(assetDock_, &QDockWidget::topLevelChanged, this,
                   suppressDuringDrag);
  QObject::connect(outputDock_, &QDockWidget::topLevelChanged, this,
                   suppressDuringDrag);

  mainWindow_->installEventFilter(new FramelessResizeHelper(mainWindow_.get()));

  mainWindow_->resize(1280, 720);

  QTimer::singleShot(0, this, [this] {
    mainWindow_->layout()->activate();
    QApplication::processEvents();
    const QByteArray state = mainWindow_->saveState();
    mainWindow_->restoreState(state);
  });
}

EditorMainWindow::~EditorMainWindow() {
  Raiden::Core::Logger::setLogSink(nullptr);
}

void EditorMainWindow::show() { mainWindow_->show(); }

void EditorMainWindow::onFrameTick() {
  if (platform_ == nullptr || context_ == nullptr) {
    return;
  }
  context_->updateEditorCamera(0.016F, platform_->getInputState());

  // update gizmo hit-testing and drag
  if (auto *overlay = context_->overlay()) {
    int vpW = 0, vpH = 0;
    platform_->getWindowSize(vpW, vpH);
    glm::mat4 view = glm::make_mat4(overlay->cameraViewMatrix());
    glm::mat4 proj = glm::make_mat4(overlay->cameraProjMatrix());
    context_->updateGizmo(platform_->getInputState(), vpW, vpH, view, proj);
  }

  if (vkWindow_ != nullptr) {
    vkWindow_->requestUpdate();
  }
}

void EditorMainWindow::onViewportTabChanged(int index) {
  if (context_ == nullptr) {
    return;
  }
  context_->setEditorMode(index == 0);
}

void EditorMainWindow::createTitleBar() {
  titleBar_ = new CustomTitleBar(mainWindow_.get());
  titleBar_->setTitleText(QStringLiteral("Raiden Editor"));
  mainWindow_->setMenuWidget(titleBar_);
}

void EditorMainWindow::createMenuBar() {
  menuBar_ = titleBar_->menuBar();

  auto *fileMenu = menuBar_->addMenu(QStringLiteral("&File"));
  fileMenu->addAction(QStringLiteral("New Scene"), this,
                      &EditorMainWindow::newScene);
  fileMenu->addAction(QStringLiteral("Open Scene..."), this,
                      &EditorMainWindow::loadScene);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("Save"), this,
                      &EditorMainWindow::saveScene);
  fileMenu->addAction(QStringLiteral("Save As..."), this,
                      &EditorMainWindow::saveSceneAs);
  fileMenu->addSeparator();
  fileMenu->addAction(QStringLiteral("Exit"), mainWindow_.get(), &QWidget::close);

  auto *editMenu = menuBar_->addMenu(QStringLiteral("&Edit"));
  editMenu->addAction(QStringLiteral("Undo"));
  editMenu->addAction(QStringLiteral("Redo"));
  editMenu->addSeparator();
  editMenu->addAction(QStringLiteral("Preferences..."));

  auto *viewMenu = menuBar_->addMenu(QStringLiteral("&View"));
  viewMenu->addAction(sceneDock_->toggleViewAction());
  viewMenu->addAction(inspectorDock_->toggleViewAction());
  viewMenu->addAction(assetDock_->toggleViewAction());
  viewMenu->addAction(outputDock_->toggleViewAction());

  auto *helpMenu = menuBar_->addMenu(QStringLiteral("&Help"));
  helpMenu->addAction(QStringLiteral("About Raiden Editor"));
}

void EditorMainWindow::createEditorToolBar() {
  auto *toolBar = new QToolBar(QStringLiteral("Editor Tools"), mainWindow_.get());
  toolBar->setObjectName(QStringLiteral("EditorToolBar"));
  toolBar->setMovable(false);
  toolBar->setIconSize(QSize(18, 18));
  toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

  auto addTool = [&](const QIcon &icon, const QString &tip) {
    auto *btn = new QToolButton(toolBar);
    btn->setIcon(icon);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setCheckable(true);
    btn->setChecked(false);
    toolBar->addWidget(btn);
    return btn;
  };

  // transform tools
  moveButton_ =
      addTool(FluentIcon::load("Move", 20, false), QStringLiteral("Move (W)"));
  rotateButton_ = addTool(FluentIcon::load("Rotate", 20, false),
                          QStringLiteral("Rotate (E)"));
  scaleButton_ =
      addTool(FluentIcon::load("Scale", 20, false), QStringLiteral("Scale (R)"));

  auto setupToolBtn = [&](QToolButton *btn, Raiden::Editor::GizmoMode mode) {
    QObject::connect(btn, &QToolButton::clicked, this, [this, btn, mode](bool checked) {
      if (!checked) {
        // was toggled off → deselect all, disable gizmo
        moveButton_->setChecked(false);
        rotateButton_->setChecked(false);
        scaleButton_->setChecked(false);
        context_->setGizmoMode(Raiden::Editor::GizmoMode::None);
      } else {
        moveButton_->setChecked(btn == moveButton_);
        rotateButton_->setChecked(btn == rotateButton_);
        scaleButton_->setChecked(btn == scaleButton_);
        context_->setGizmoMode(mode);
      }
    });
  };
  setupToolBtn(moveButton_, Raiden::Editor::GizmoMode::Translate);
  setupToolBtn(rotateButton_, Raiden::Editor::GizmoMode::Rotate);
  setupToolBtn(scaleButton_, Raiden::Editor::GizmoMode::Scale);
  moveButton_->setChecked(true);

  // keyboard shortcuts for gizmo modes (1/2/3 to avoid WASD conflict)
  auto *shortcut1 = new QShortcut(QKeySequence(Qt::Key_1), mainWindow_.get());
  auto *shortcut2 = new QShortcut(QKeySequence(Qt::Key_2), mainWindow_.get());
  auto *shortcut3 = new QShortcut(QKeySequence(Qt::Key_3), mainWindow_.get());
  QObject::connect(shortcut1, &QShortcut::activated, this, [this]() {
    moveButton_->click();
  });
  QObject::connect(shortcut2, &QShortcut::activated, this, [this]() {
    rotateButton_->click();
  });
  QObject::connect(shortcut3, &QShortcut::activated, this, [this]() {
    scaleButton_->click();
  });

  toolBar->addSeparator();

  // playback controls
  playButton_ = addTool(FluentIcon::load("Play"), QStringLiteral("Play"));
  pauseButton_ = addTool(FluentIcon::load("Pause"), QStringLiteral("Pause"));
  stopButton_ = addTool(FluentIcon::load("Stop"), QStringLiteral("Stop"));

  toolBar->addSeparator();
  addTool(FluentIcon::load("Wrench"), QStringLiteral("Build"));

  editorToolBar_ = toolBar;
  mainWindow_->addToolBar(Qt::LeftToolBarArea, editorToolBar_);
}

void EditorMainWindow::createSceneDock() {
  sceneDock_ = new QDockWidget(QStringLiteral("Scene Hierarchy"), mainWindow_.get());
  sceneDock_->setObjectName(QStringLiteral("SceneHierarchyDock"));
  sceneDock_->setFeatures(QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetFloatable);

  auto *container = new QWidget();
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *searchBox = new QLineEdit();
  searchBox->setObjectName(QStringLiteral("SceneSearchBox"));
  searchBox->setPlaceholderText(QStringLiteral("Search objects..."));
  searchBox->addAction(FluentIcon::load("Search", 16, false),
                       QLineEdit::LeadingPosition);
  layout->addWidget(searchBox);

  auto *tree = new QTreeWidget();
  tree->setHeaderHidden(true);
  tree->setRootIsDecorated(false);
  tree->setAnimated(true);
  tree->setIndentation(0);
  tree->setIconSize(QSize(16, 16));

  sceneTree_ = tree;
  layout->addWidget(tree, 1);
  sceneDock_->setWidget(container);
  mainWindow_->addDockWidget(Qt::LeftDockWidgetArea, sceneDock_);

  // connect tree selection -> context
  QObject::connect(tree, &QTreeWidget::itemSelectionChanged, this,
                   [this, tree]() {
                     auto *item = tree->currentItem();
                     if (item == nullptr) {
                       context_->setSelection(Raiden::ECS::NullEntity);
                       return;
                     }
                     uint32_t idx = item->data(0, Qt::UserRole).toUInt();
                     context_->setSelection(Raiden::ECS::Entity{idx, 0});
                   });

  // rebuild tree when world changes
  QObject::connect(context_, &EditorContext::worldChanged, this,
                   [this](Raiden::ECS::World *) { rebuildSceneTree(); });
}

void EditorMainWindow::createInspectorDock() {
  inspectorDock_ = new QDockWidget(QStringLiteral("Properties"), mainWindow_.get());
  inspectorDock_->setObjectName(QStringLiteral("InspectorDock"));
  inspectorDock_->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetFloatable);

  auto *tabs = new QTabWidget();
  tabs->setDocumentMode(true);

  // inspector tab
  auto createAxisField = [](const QString &axis, const QColor &color,
                            double value) -> QWidget * {
    auto *field = new QWidget();
    auto *hl = new QHBoxLayout(field);
    hl->setContentsMargins(4, 2, 4, 2);
    hl->setSpacing(2);

    auto *label = new QLabel(axis);
    label->setObjectName(QStringLiteral("AxisLetter"));

    auto *spin = new QDoubleSpinBox();
    spin->setRange(-99999.0, 99999.0);
    spin->setValue(value);
    spin->setSingleStep(0.1);
    spin->setFrame(false);
    spin->setObjectName(QStringLiteral("AxisValue"));
    spin->setStyleSheet(
        QStringLiteral("QDoubleSpinBox { background: transparent; border: "
                       "none; color: #dddddd; font-size: 11px; "
                       "padding: 0px; min-height: 0px; max-height: 999px; } "
                       "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button "
                       "{ width: 10px; border: none; background: transparent; "
                       "}"));

    hl->addWidget(label);
    hl->addWidget(spin, 1);

    field->setStyleSheet(
        QStringLiteral("background: #2b2b2b; border: 1px solid #050505; "
                       "border-left: 2px solid %1; border-radius: 2px;")
            .arg(color.name()));
    return field;
  };

  auto createVec3Row = [&](const QString &label, double x, double y,
                           double z) -> QWidget * {
    auto *row = new QWidget();
    auto *hl = new QHBoxLayout(row);
    hl->setContentsMargins(12, 2, 12, 2);
    hl->setSpacing(4);

    auto *lbl = new QLabel(label);
    if (label == QStringLiteral("P")) {
      lbl->setObjectName(QStringLiteral("TransformP"));
    } else if (label == QStringLiteral("R")) {
      lbl->setObjectName(QStringLiteral("TransformR"));
    } else {
      lbl->setObjectName(QStringLiteral("TransformS"));
    }

    lbl->setFixedWidth(16);
    lbl->setAlignment(Qt::AlignCenter);

    hl->addWidget(lbl);
    hl->addWidget(
        createAxisField(QStringLiteral("X"), QColor(0xE2, 0x55, 0x4F), x), 1);
    hl->addWidget(
        createAxisField(QStringLiteral("Y"), QColor(0x4F, 0xAE, 0x5F), y), 1);
    hl->addWidget(
        createAxisField(QStringLiteral("Z"), QColor(0x4C, 0x8B, 0xF5), z), 1);

    return row;
  };

  auto *inspectorScroll = new QScrollArea();
  inspectorScroll->setWidgetResizable(true);
  inspectorScroll->setFrameShape(QFrame::NoFrame);

  auto *inspectorContent = new QWidget();
  auto *inspectorLayout = new QVBoxLayout(inspectorContent);
  inspectorLayout->setContentsMargins(0, 0, 0, 0);
  inspectorLayout->setSpacing(0);

  auto *transformHeader = new QLabel(QStringLiteral("Transform"));
  transformHeader->setObjectName(QStringLiteral("SectionHeader"));
  inspectorLayout->addWidget(transformHeader);

  inspectorLayout->addWidget(createVec3Row(QStringLiteral("P"), 0.0, 1.0, 0.0));
  inspectorLayout->addWidget(
      createVec3Row(QStringLiteral("R"), 0.0, 45.0, 0.0));
  inspectorLayout->addWidget(createVec3Row(QStringLiteral("S"), 1.0, 1.0, 1.0));

  auto *card = new QGroupBox();
  card->setObjectName(QStringLiteral("ComponentCard"));
  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  auto *cardHeader = new QWidget();
  cardHeader->setObjectName(QStringLiteral("ComponentHeader"));
  auto *cardHeaderLayout = new QHBoxLayout(cardHeader);
  cardHeaderLayout->setContentsMargins(8, 5, 4, 5);
  cardHeaderLayout->setSpacing(0);

  auto *cardTitle = new QLabel(QStringLiteral("Mesh Renderer"));
  cardTitle->setObjectName(QStringLiteral("ComponentTitle"));
  cardHeaderLayout->addWidget(cardTitle, 1);

  auto *cardMenu = new QPushButton();
  cardMenu->setIcon(FluentIcon::load("MoreHorizontal", 20, false));
  cardMenu->setIconSize(QSize(16, 16));
  cardMenu->setObjectName(QStringLiteral("ComponentMenuButton"));
  cardMenu->setFixedSize(28, 28);
  cardMenu->setFlat(true);
  cardHeaderLayout->addWidget(cardMenu);

  cardLayout->addWidget(cardHeader);

  auto *cardBody = new QWidget();
  cardBody->setObjectName(QStringLiteral("ComponentBody"));
  auto *cardBodyLayout = new QFormLayout(cardBody);
  cardBodyLayout->setContentsMargins(8, 6, 8, 6);
  cardBodyLayout->setSpacing(4);

  auto *meshLbl = new QLabel(QStringLiteral("Mesh"));
  meshLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *meshVal = new QLabel(QStringLiteral("Mesh_Player.fbx"));
  meshVal->setObjectName(QStringLiteral("FieldValue"));
  cardBodyLayout->addRow(meshLbl, meshVal);

  auto *matLbl = new QLabel(QStringLiteral("Material"));
  matLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *matVal = new QLabel(QStringLiteral("PBR_Default"));
  matVal->setObjectName(QStringLiteral("FieldValue"));
  cardBodyLayout->addRow(matLbl, matVal);

  cardLayout->addWidget(cardBody);
  inspectorLayout->addWidget(card);

  // rigidbody component card
  auto *rbCard = new QGroupBox();
  rbCard->setObjectName(QStringLiteral("ComponentCard"));
  auto *rbCardLayout = new QVBoxLayout(rbCard);
  rbCardLayout->setContentsMargins(0, 0, 0, 0);
  rbCardLayout->setSpacing(0);

  auto *rbHeader = new QWidget();
  rbHeader->setObjectName(QStringLiteral("ComponentHeader"));
  auto *rbHeaderLayout = new QHBoxLayout(rbHeader);
  rbHeaderLayout->setContentsMargins(8, 5, 4, 5);
  rbHeaderLayout->setSpacing(0);

  auto *rbTitle = new QLabel(QStringLiteral("Rigidbody"));
  rbTitle->setObjectName(QStringLiteral("ComponentTitle"));
  rbHeaderLayout->addWidget(rbTitle, 1);

  auto *rbMenu = new QPushButton();
  rbMenu->setIcon(FluentIcon::load("MoreHorizontal", 20, false));
  rbMenu->setIconSize(QSize(16, 16));
  rbMenu->setObjectName(QStringLiteral("ComponentMenuButton"));
  rbMenu->setFixedSize(28, 28);
  rbMenu->setFlat(true);
  rbHeaderLayout->addWidget(rbMenu);

  rbCardLayout->addWidget(rbHeader);

  auto *rbBody = new QWidget();
  rbBody->setObjectName(QStringLiteral("ComponentBody"));
  auto *rbBodyLayout = new QFormLayout(rbBody);
  rbBodyLayout->setContentsMargins(8, 6, 8, 6);
  rbBodyLayout->setSpacing(4);

  auto *rbTypeLbl = new QLabel(QStringLiteral("Type"));
  rbTypeLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *rbTypeVal = new QLabel(QStringLiteral("Dynamic"));
  rbTypeVal->setObjectName(QStringLiteral("FieldValue"));
  rbBodyLayout->addRow(rbTypeLbl, rbTypeVal);

  auto *rbMassLbl = new QLabel(QStringLiteral("Mass"));
  rbMassLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *rbMassVal = new QLabel(QStringLiteral("1.0"));
  rbMassVal->setObjectName(QStringLiteral("FieldValue"));
  rbBodyLayout->addRow(rbMassLbl, rbMassVal);

  auto *rbFrictionLbl = new QLabel(QStringLiteral("Friction"));
  rbFrictionLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *rbFrictionVal = new QLabel(QStringLiteral("0.5"));
  rbFrictionVal->setObjectName(QStringLiteral("FieldValue"));
  rbBodyLayout->addRow(rbFrictionLbl, rbFrictionVal);

  rbCardLayout->addWidget(rbBody);
  inspectorLayout->addWidget(rbCard);

  // collider component card
  auto *colCard = new QGroupBox();
  colCard->setObjectName(QStringLiteral("ComponentCard"));
  auto *colCardLayout = new QVBoxLayout(colCard);
  colCardLayout->setContentsMargins(0, 0, 0, 0);
  colCardLayout->setSpacing(0);

  auto *colHeader = new QWidget();
  colHeader->setObjectName(QStringLiteral("ComponentHeader"));
  auto *colHeaderLayout = new QHBoxLayout(colHeader);
  colHeaderLayout->setContentsMargins(8, 5, 4, 5);
  colHeaderLayout->setSpacing(0);

  auto *colTitle = new QLabel(QStringLiteral("Collider"));
  colTitle->setObjectName(QStringLiteral("ComponentTitle"));
  colHeaderLayout->addWidget(colTitle, 1);

  auto *colMenuBtn = new QPushButton();
  colMenuBtn->setIcon(FluentIcon::load("MoreHorizontal", 20, false));
  colMenuBtn->setIconSize(QSize(16, 16));
  colMenuBtn->setObjectName(QStringLiteral("ComponentMenuButton"));
  colMenuBtn->setFixedSize(28, 28);
  colMenuBtn->setFlat(true);
  colHeaderLayout->addWidget(colMenuBtn);

  colCardLayout->addWidget(colHeader);

  auto *colBody = new QWidget();
  colBody->setObjectName(QStringLiteral("ComponentBody"));
  auto *colBodyLayout = new QFormLayout(colBody);
  colBodyLayout->setContentsMargins(8, 6, 8, 6);
  colBodyLayout->setSpacing(4);

  auto *colShapeLbl = new QLabel(QStringLiteral("Shape"));
  colShapeLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *colShapeVal = new QLabel(QStringLiteral("Box"));
  colShapeVal->setObjectName(QStringLiteral("FieldValue"));
  colBodyLayout->addRow(colShapeLbl, colShapeVal);

  auto *colSizeLbl = new QLabel(QStringLiteral("Half Extents"));
  colSizeLbl->setObjectName(QStringLiteral("FieldLabel"));
  auto *colSizeVal = new QLabel(QStringLiteral("0.5, 0.5, 0.5"));
  colSizeVal->setObjectName(QStringLiteral("FieldValue"));
  colBodyLayout->addRow(colSizeLbl, colSizeVal);

  colCardLayout->addWidget(colBody);
  inspectorLayout->addWidget(colCard);

  // add component button
  auto *addBtn = new QPushButton(QStringLiteral("  Add component"));
  addBtn->setIcon(FluentIcon::load("Add", 16, false));
  addBtn->setIconSize(QSize(14, 14));
  addBtn->setObjectName(QStringLiteral("AddComponentButton"));
  addBtn->setCursor(Qt::PointingHandCursor);
  inspectorLayout->addWidget(addBtn);

  inspectorLayout->addStretch(1);
  inspectorScroll->setWidget(inspectorContent);
  tabs->addTab(inspectorScroll, QStringLiteral("Inspector"));

  // world settings tab
  auto *worldScroll = new QScrollArea();
  worldScroll->setWidgetResizable(true);
  worldScroll->setFrameShape(QFrame::NoFrame);

  auto *worldContent = new QWidget();
  auto *worldLayout = new QVBoxLayout(worldContent);
  worldLayout->setContentsMargins(0, 0, 0, 0);
  worldLayout->setSpacing(0);

  auto *envCard = new QGroupBox();
  envCard->setObjectName(QStringLiteral("ComponentCard"));
  auto *envCardLayout = new QVBoxLayout(envCard);
  envCardLayout->setContentsMargins(0, 0, 0, 0);
  envCardLayout->setSpacing(0);

  auto *envHeader = new QWidget();
  envHeader->setObjectName(QStringLiteral("ComponentHeader"));
  auto *envHeaderLayout = new QHBoxLayout(envHeader);
  envHeaderLayout->setContentsMargins(8, 5, 4, 5);
  envHeaderLayout->setSpacing(0);

  auto *envTitle = new QLabel(QStringLiteral("Environment"));
  envTitle->setObjectName(QStringLiteral("ComponentTitle"));
  envHeaderLayout->addWidget(envTitle, 1);

  envCardLayout->addWidget(envHeader);

  auto *envBody = new QWidget();
  envBody->setObjectName(QStringLiteral("ComponentBody"));
  auto *envRows = new QFormLayout(envBody);
  envRows->setContentsMargins(8, 6, 8, 6);
  envRows->setSpacing(4);

  auto *gravitySpin = new QDoubleSpinBox();
  gravitySpin->setRange(-100.0, 0.0);
  gravitySpin->setValue(-9.81);
  envRows->addRow(QStringLiteral("Gravity"), gravitySpin);

  auto *skyLabel = new QLabel(QStringLiteral("Sky_Default"));
  envRows->addRow(QStringLiteral("Skybox"), skyLabel);

  envCardLayout->addWidget(envBody);
  worldLayout->addWidget(envCard);

  // physics config card
  auto *physCard = new QGroupBox();
  physCard->setObjectName(QStringLiteral("ComponentCard"));
  auto *physCardLayout = new QVBoxLayout(physCard);
  physCardLayout->setContentsMargins(0, 0, 0, 0);
  physCardLayout->setSpacing(0);

  auto *physHeader = new QWidget();
  physHeader->setObjectName(QStringLiteral("ComponentHeader"));
  auto *physHeaderLayout = new QHBoxLayout(physHeader);
  physHeaderLayout->setContentsMargins(8, 5, 4, 5);
  physHeaderLayout->setSpacing(0);

  auto *physTitle = new QLabel(QStringLiteral("Physics"));
  physTitle->setObjectName(QStringLiteral("ComponentTitle"));
  physHeaderLayout->addWidget(physTitle, 1);

  physCardLayout->addWidget(physHeader);

  auto *physBody = new QWidget();
  physBody->setObjectName(QStringLiteral("ComponentBody"));
  auto *physRows = new QFormLayout(physBody);
  physRows->setContentsMargins(8, 6, 8, 6);
  physRows->setSpacing(4);

  auto *timestepSpin = new QDoubleSpinBox();
  timestepSpin->setRange(0.001, 0.1);
  timestepSpin->setValue(1.0 / 60.0);
  timestepSpin->setSingleStep(0.001);
  timestepSpin->setDecimals(4);
  physRows->addRow(QStringLiteral("Fixed Timestep"), timestepSpin);

  auto *solverPosSpin = new QSpinBox();
  solverPosSpin->setRange(1, 16);
  solverPosSpin->setValue(2);
  physRows->addRow(QStringLiteral("Solver Positions"), solverPosSpin);

  auto *solverVelSpin = new QSpinBox();
  solverVelSpin->setRange(1, 32);
  solverVelSpin->setValue(4);
  physRows->addRow(QStringLiteral("Solver Velocities"), solverVelSpin);

  auto *maxBodiesSpin = new QSpinBox();
  maxBodiesSpin->setRange(16, 65536);
  maxBodiesSpin->setValue(1024);
  maxBodiesSpin->setSingleStep(64);
  physRows->addRow(QStringLiteral("Max Bodies"), maxBodiesSpin);

  physCardLayout->addWidget(physBody);
  worldLayout->addWidget(physCard);

  worldLayout->addStretch(1);
  worldScroll->setWidget(worldContent);
  tabs->addTab(worldScroll, QStringLiteral("World Settings"));

  inspectorDock_->setWidget(tabs);
  mainWindow_->addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void EditorMainWindow::createAssetDock() {
  assetDock_ = new QDockWidget(QStringLiteral("Asset Browser"), mainWindow_.get());
  assetDock_->setObjectName(QStringLiteral("AssetBrowserDock"));
  assetDock_->setFeatures(QDockWidget::DockWidgetMovable |
                          QDockWidget::DockWidgetClosable |
                          QDockWidget::DockWidgetFloatable);

  auto *container = new QWidget();
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // toolbar row
  auto *toolbarRow = new QWidget();
  auto *toolbarLayout = new QHBoxLayout(toolbarRow);
  toolbarLayout->setContentsMargins(4, 4, 4, 4);
  toolbarLayout->setSpacing(6);

  auto *breadcrumb = new QLabel();
  breadcrumb->setObjectName(QStringLiteral("Breadcrumb"));
  breadcrumb->setTextFormat(Qt::RichText);

  auto *searchBox = new QLineEdit();
  searchBox->setPlaceholderText(QStringLiteral("Search assets..."));
  searchBox->setClearButtonEnabled(true);
  searchBox->setMaximumWidth(220);
  searchBox->addAction(FluentIcon::load("Search", 16, false),
                       QLineEdit::LeadingPosition);

  toolbarLayout->addWidget(breadcrumb);
  toolbarLayout->addStretch(1);
  toolbarLayout->addWidget(searchBox);
  layout->addWidget(toolbarRow);

  // splitter: tree | grid
  auto *splitter = new QSplitter(Qt::Horizontal);
  splitter->setHandleWidth(1);
  splitter->setChildrenCollapsible(false);

  // left: folder tree
  auto *tree = new QTreeWidget();
  tree->setObjectName(QStringLiteral("AssetTree"));
  tree->setHeaderHidden(true);
  tree->setRootIsDecorated(true);
  tree->setIconSize(QSize(16, 16));
  tree->setIndentation(14);
  tree->setMinimumWidth(130);
  tree->setMaximumWidth(280);

  // right: content grid
  auto *list = new QListWidget();
  list->setObjectName(QStringLiteral("AssetGrid"));
  list->setViewMode(QListView::IconMode);
  list->setIconSize(QSize(48, 48));
  list->setGridSize(QSize(90, 100));
  list->setResizeMode(QListView::Adjust);
  list->setWordWrap(true);
  list->setSpacing(2);

  splitter->addWidget(tree);
  splitter->addWidget(list);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  layout->addWidget(splitter, 1);

  QString rootPath = QString::fromStdString(datapackPath_);

  // recursively populate tree with subdirectories
  std::function<void(QTreeWidgetItem *, const QString &)> populateTree;
  populateTree = [&](QTreeWidgetItem *parent, const QString &dirPath) {
    QDir dir(dirPath);
    auto entries =
        dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &info : entries) {
      auto *item = new QTreeWidgetItem(parent);
      item->setText(0, info.fileName());
      item->setIcon(0, FluentIcon::load("DocumentFolder", 20, false));
      item->setData(0, Qt::UserRole, info.absoluteFilePath());
      populateTree(item, info.absoluteFilePath());
    }
  };

  auto *rootItem = new QTreeWidgetItem(tree);
  rootItem->setText(0, QDir(rootPath).dirName());
  rootItem->setIcon(0, FluentIcon::load("DocumentFolder", 20, false));
  rootItem->setData(0, Qt::UserRole, rootPath);
  rootItem->setExpanded(true);
  populateTree(rootItem, rootPath);

  // icon resolver: thumbnails for images, fluent icons for everything else
  auto iconFor = [](const QString &fullPath) -> QIcon {
    QString lower = fullPath.toLower();
    if (lower.endsWith(QStringLiteral(".png")) ||
        lower.endsWith(QStringLiteral(".jpg")) ||
        lower.endsWith(QStringLiteral(".jpeg")) ||
        lower.endsWith(QStringLiteral(".ktx")) ||
        lower.endsWith(QStringLiteral(".ktx2"))) {
      QImage img(fullPath);
      if (!img.isNull()) {
        return QIcon(QPixmap::fromImage(
            img.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
      }
    }
    if (lower.endsWith(QStringLiteral(".fbx")) ||
        lower.endsWith(QStringLiteral(".glb")) ||
        lower.endsWith(QStringLiteral(".gltf")) ||
        lower.endsWith(QStringLiteral(".obj"))) {
      return FluentIcon::load("Cube", 20, false);
    }
    if (lower.endsWith(QStringLiteral(".glsl")) ||
        lower.endsWith(QStringLiteral(".hlsl")) ||
        lower.endsWith(QStringLiteral(".spv")) ||
        lower.endsWith(QStringLiteral(".toml"))) {
      return FluentIcon::load("Code", 20, false);
    }
    if (lower.endsWith(QStringLiteral(".wav")) ||
        lower.endsWith(QStringLiteral(".mp3")) ||
        lower.endsWith(QStringLiteral(".ogg"))) {
      return FluentIcon::load("MusicNote1", 20, false);
    }
    if (lower.endsWith(QStringLiteral(".ttf")) ||
        lower.endsWith(QStringLiteral(".otf"))) {
      return FluentIcon::load("Folder", 20, false);
    }
    if (lower.endsWith(QStringLiteral(".svg"))) {
      return FluentIcon::load("Info", 20, false);
    }
    return FluentIcon::load("DocumentFolder", 20, false);
  };

  // load directory contents into grid
  auto loadDir = [=](const QString &dirPath) {
    list->clear();

    // breadcrumb
    QString rel = QDir(rootPath).relativeFilePath(dirPath);

    auto buildBreadcrumb = [](const QStringList &segments) {
      QString html;
      html += QStringLiteral(
          "<span style='color:#6a6a6a;'>Content</span>"
          " <span style='color:#3a3a3a; font-size:8px;'>\u25B8</span>"
          " <span style='color:#bbbbbb;'>Game</span>");
      for (const auto &seg : segments) {
        if (seg.isEmpty()) {
          continue;
        }
        html += QStringLiteral(
                    " <span style='color:#3a3a3a; font-size:8px;'>\u25B8</span>"
                    " <span style='color:#888888;'>%1</span>")
                    .arg(seg.toHtmlEscaped());
      }
      return html;
    };

    if (rel == QStringLiteral(".")) {
      breadcrumb->setText(buildBreadcrumb({}));
    } else {
      breadcrumb->setText(buildBreadcrumb(rel.split(QStringLiteral("/"))));
    }

    QDir dir(dirPath);
    auto entries =
        dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                          QDir::DirsFirst | QDir::Name);

    for (const auto &info : entries) {
      bool isDir = info.isDir();
      QString label =
          isDir ? info.fileName() + QStringLiteral("/") : info.fileName();

      QIcon ic = isDir ? FluentIcon::load("DocumentFolder", 20, false)
                       : iconFor(info.absoluteFilePath());

      auto *item = new QListWidgetItem(ic, label);
      item->setData(Qt::UserRole, info.absoluteFilePath());
      item->setData(Qt::UserRole + 1, isDir);

      if (isDir) {
        item->setToolTip(QStringLiteral("Directory"));
      } else {
        item->setToolTip(QStringLiteral("%1  (%2 KB)")
                             .arg(info.fileName())
                             .arg(info.size() / 1024));
      }
      list->addItem(item);
    }
  };

  // connect tree selection -> load grid
  QObject::connect(tree, &QTreeWidget::currentItemChanged, this,
                   [loadDir](QTreeWidgetItem *current, QTreeWidgetItem *) {
                     if (current) {
                       loadDir(current->data(0, Qt::UserRole).toString());
                     }
                   });

  // double-click directory in grid -> navigate
  QObject::connect(list, &QListWidget::itemDoubleClicked, this,
                   [tree, loadDir](QListWidgetItem *item) {
                     if (item->data(Qt::UserRole + 1).toBool()) {
                       QString path = item->data(Qt::UserRole).toString();
                       loadDir(path);
                       QTreeWidgetItemIterator it(tree);
                       while (*it) {
                         if ((*it)->data(0, Qt::UserRole).toString() == path) {
                           tree->setCurrentItem(*it);
                           break;
                         }
                         ++it;
                       }
                     }
                   });

  // search filter
  QObject::connect(
      searchBox, &QLineEdit::textChanged, this, [list](const QString &text) {
        for (int i = 0; i < list->count(); ++i) {
          auto *item = list->item(i);
          item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
      });

  // initial load
  loadDir(rootPath);

  assetTree_ = tree;
  assetList_ = list;
  assetDock_->setWidget(container);
  mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, assetDock_);
}

void EditorMainWindow::createOutputDock() {
  outputDock_ = new QDockWidget(QStringLiteral("Output"), mainWindow_.get());
  outputDock_->setObjectName(QStringLiteral("OutputDock"));
  outputDock_->setFeatures(QDockWidget::DockWidgetMovable |
                           QDockWidget::DockWidgetClosable |
                           QDockWidget::DockWidgetFloatable);

  auto *log = new QPlainTextEdit();
  log->setReadOnly(true);
  log->setPlaceholderText(QStringLiteral("Editor log output..."));

  outputLog_ = log;
  outputDock_->setWidget(log);

  Raiden::Core::Logger::setLogSink(
      [log](Raiden::Core::LogLevel level, std::string_view tag,
            std::string_view message, std::string_view timeStr) {
        auto tagQ = QString::fromUtf8(tag.data(), static_cast<int>(tag.size()));
        auto timeQ =
            QString::fromUtf8(timeStr.data(), static_cast<int>(timeStr.size()));
        auto msgQ =
            QString::fromUtf8(message.data(), static_cast<int>(message.size()));

        auto levelQ = QStringLiteral("DEBUG");
        QColor levelColor;
        switch (level) {
        case Raiden::Core::LogLevel::Debug:
          levelColor = QColor(0x9E, 0x9E, 0x9E);
          break;
        case Raiden::Core::LogLevel::Info:
          levelQ = QStringLiteral("INFO");
          levelColor = QColor(0x81, 0xC7, 0x84);
          break;
        case Raiden::Core::LogLevel::Warning:
          levelQ = QStringLiteral("WARN");
          levelColor = QColor(0xFF, 0xD5, 0x4F);
          break;
        case Raiden::Core::LogLevel::Error:
          levelQ = QStringLiteral("ERROR");
          levelColor = QColor(0xEF, 0x53, 0x50);
          break;
        case Raiden::Core::LogLevel::Critical:
          levelQ = QStringLiteral("CRITICAL");
          levelColor = QColor(0xFF, 0x17, 0x44);
          break;
        }

        QMetaObject::invokeMethod(
            log,
            [log, tagQ, timeQ, levelQ, msgQ, levelColor] {
              QTextCursor cursor(log->document());
              cursor.movePosition(QTextCursor::End);

              QTextCharFormat tagFmt;
              tagFmt.setForeground(QColor(0x4D, 0xD0, 0xE1));
              cursor.insertText(QStringLiteral("[%1] ").arg(tagQ), tagFmt);

              QTextCharFormat timeFmt;
              timeFmt.setForeground(QColor(0x9E, 0x9E, 0x9E));
              cursor.insertText(QStringLiteral("[%1] [").arg(timeQ), timeFmt);

              QTextCharFormat levelFmt;
              levelFmt.setForeground(levelColor);
              if (levelColor == QColor(0xFF, 0x17, 0x44)) {
                QFont boldFont = levelFmt.font();
                boldFont.setBold(true);
                levelFmt.setFont(boldFont);
              }
              cursor.insertText(levelQ, levelFmt);

              QTextCharFormat defaultFmt;
              defaultFmt.setForeground(QColor(0x9E, 0x9E, 0x9E));
              cursor.insertText(QStringLiteral("] "), defaultFmt);

              QTextCharFormat msgFmt;
              msgFmt.setForeground(QColor(0xCC, 0xCC, 0xCC));
              cursor.insertText(msgQ, msgFmt);

              cursor.insertText(QStringLiteral("\n"), defaultFmt);

              auto *sb = log->verticalScrollBar();
              sb->setValue(sb->maximum());
            },
            Qt::QueuedConnection);
      });
}

// status bar
void EditorMainWindow::createStatusBar() {
  statusBar_ = mainWindow_->statusBar();
  statusBar_->showMessage(QStringLiteral("Ready"));
}

// scene save/load

static Raiden::Scene::SerializedScene
serializeWorldExcept(Raiden::ECS::World &world, Raiden::ECS::Entity /*exclude*/) {
  auto scene = Raiden::Scene::serializeWorld(world);
  // remove the excluded entity (editor camera) from the serialized data
  for (auto it = scene.entities.begin(); it != scene.entities.end();) {
    if (it->name == "Editor Camera") {
      it = scene.entities.erase(it);
    } else {
      ++it;
    }
  }
  return scene;
}

void EditorMainWindow::saveScene() {
  auto *world = context_->world();
  if (world == nullptr) {
    QMessageBox::warning(mainWindow_.get(), QStringLiteral("Save Scene"),
                         QStringLiteral("No world loaded."));
    return;
  }

  const auto &path = context_->currentScenePath();
  if (path.empty()) {
    saveSceneAs();
    return;
  }

  auto scene = serializeWorldExcept(*world, context_->editorCameraEntity());
  if (Raiden::Scene::saveJson(scene, *vfs_, path)) {
    context_->clearDirty();
    statusBar_->showMessage(
        QStringLiteral("Scene saved: %1").arg(QString::fromStdString(path)),
        3000);
  } else {
    QMessageBox::critical(mainWindow_.get(), QStringLiteral("Save Scene"),
                          QStringLiteral("Failed to save scene."));
  }
}

void EditorMainWindow::saveSceneAs() {
  auto *world = context_->world();
  if (world == nullptr) {
    QMessageBox::warning(mainWindow_.get(), QStringLiteral("Save Scene"),
                         QStringLiteral("No world loaded."));
    return;
  }

  QString path = QFileDialog::getSaveFileName(
      mainWindow_.get(), QStringLiteral("Save Scene As"),
      QString::fromStdString(datapackPath_),
      QStringLiteral("Scene files (*.scene.json);;All files (*)"));

  if (path.isEmpty()) {
    return;
  }

  auto scene = serializeWorldExcept(*world, context_->editorCameraEntity());
  std::string pathStr = path.toStdString();
  if (Raiden::Scene::saveJson(scene, *vfs_, pathStr)) {
    context_->setCurrentScenePath(pathStr);
    statusBar_->showMessage(QStringLiteral("Scene saved: %1").arg(path), 3000);
  } else {
    QMessageBox::critical(mainWindow_.get(), QStringLiteral("Save Scene"),
                          QStringLiteral("Failed to save scene."));
  }
}

void EditorMainWindow::loadScene() {
  auto *world = context_->world();
  if (world == nullptr) {
    QMessageBox::warning(mainWindow_.get(), QStringLiteral("Open Scene"),
                         QStringLiteral("No world loaded."));
    return;
  }

  QString path = QFileDialog::getOpenFileName(
      mainWindow_.get(), QStringLiteral("Open Scene"),
      QString::fromStdString(datapackPath_),
      QStringLiteral("Scene files (*.scene.json);;All files (*)"));

  if (path.isEmpty()) {
    return;
  }

  Raiden::Scene::SerializedScene scene;
  std::string pathStr = path.toStdString();
  if (!Raiden::Scene::loadJson(scene, *vfs_, pathStr)) {
    QMessageBox::critical(mainWindow_.get(), QStringLiteral("Open Scene"),
                          QStringLiteral("Failed to load scene."));
    return;
  }

  // clear existing entities (preserve editor camera)
  auto editorCam = context_->editorCameraEntity();
  auto view = world->view<Raiden::ECS::Transform>();
  std::vector<Raiden::ECS::Entity> toDestroy;
  view.each([&](Raiden::ECS::Entity e, Raiden::ECS::Transform &) {
    if (e != editorCam) {
      toDestroy.push_back(e);
    }
  });
  for (auto &e : toDestroy) {
    world->destroy(e);
  }

  Raiden::Scene::deserializeWorld(scene, *world);
  context_->setCurrentScenePath(pathStr);
  context_->setSelection(Raiden::ECS::NullEntity);
  rebuildSceneTree();
  statusBar_->showMessage(QStringLiteral("Scene loaded: %1").arg(path), 3000);
}

void EditorMainWindow::newScene() {
  auto *world = context_->world();
  if (world == nullptr) {
    return;
  }

  if (context_->isDirty()) {
    auto ret = QMessageBox::question(
        mainWindow_.get(), QStringLiteral("New Scene"),
        QStringLiteral("Unsaved changes will be lost. Continue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return;
    }
  }

  // clear all entities (preserve editor camera)
  auto editorCam = context_->editorCameraEntity();
  auto view = world->view<Raiden::ECS::Transform>();
  std::vector<Raiden::ECS::Entity> toDestroy;
  view.each([&](Raiden::ECS::Entity e, Raiden::ECS::Transform &) {
    if (e != editorCam) {
      toDestroy.push_back(e);
    }
  });
  for (auto &e : toDestroy) {
    world->destroy(e);
  }

  context_->setCurrentScenePath("");
  context_->setSelection(Raiden::ECS::NullEntity);
  rebuildSceneTree();
  statusBar_->showMessage(QStringLiteral("New scene"), 3000);
}

void EditorMainWindow::rebuildSceneTree() {
  if (sceneTree_ == nullptr) {
    return;
  }

  sceneTree_->clear();

  auto *world = context_->world();
  if (world == nullptr) {
    return;
  }

  auto loadIcon = [](const QString &name) {
    return FluentIcon::load(name, 20, false);
  };

  // collect entities with Name (or fallback label), skip editor camera
  struct Entry {
    Raiden::ECS::Entity entity;
    QString name;
  };
  std::vector<Entry> entries;

  auto editorCam = context_->editorCameraEntity();

  world->view<Raiden::ECS::Name>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Name &n) {
        if (e != editorCam) {
          entries.push_back({e, QString::fromStdString(n.value)});
        }
      });

  // entities without Name but with Transform
  world->view<Raiden::ECS::Transform>().each(
      [&](Raiden::ECS::Entity e, Raiden::ECS::Transform &) {
        if (e == editorCam) {
          return;
        }
        bool hasName = false;
        world->view<Raiden::ECS::Name>().each(
            [&](Raiden::ECS::Entity ne, Raiden::ECS::Name &) {
              if (ne == e) {
                hasName = true;
              }
            });
        if (!hasName) {
          entries.push_back({e, QStringLiteral("Entity %1").arg(e.index)});
        }
      });

  for (auto &[entity, name] : entries) {
    auto *item = new QTreeWidgetItem(sceneTree_);
    item->setText(0, name);
    item->setIcon(0, loadIcon("Cube"));
    item->setData(0, Qt::UserRole, QVariant::fromValue(entity.index));
  }
}

} // namespace RaidenEditor
