#include <Raiden/Application.hpp>
#include <Raiden/Core/ConfigLoader.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/ECS/Name.hpp>
#include <Raiden/ECS/World.hpp>
#include <Raiden/Logger.hpp>
#include <Raiden/Renderer/ICommandBuffer.hpp>

#include <Raiden/Platform/Qt/QtPlatform.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>

#include <RaidenEditor/EditorMainWindow.hpp>
#include <RaidenEditor/Core/EditorContext.hpp>
#include <RaidenEditor/Widgets/ProjectLauncherDialog.hpp>

#include <QApplication>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QProxyStyle>
#include <QString>
#include <QStyleFactory>
#include <QTextStream>

#include <cstring>
#include <filesystem>
#include <memory>

static const Raiden::Core::Logger s_logger("EditorRuntime");

static void loadInterFont() {
  auto loadFont = [](const QString &path, int pointSize) -> bool {
    int id = QFontDatabase::addApplicationFont(path);
    if (id >= 0) {
      QStringList families = QFontDatabase::applicationFontFamilies(id);
      if (!families.isEmpty()) {
        QFont font(families.first());
        font.setPointSize(pointSize);
        QApplication::setFont(font);
        return true;
      }
    }
    return false;
  };

  // try IBM Plex Sans first, fall back to Inter, then system
  if (loadFont(QStringLiteral(":/editor/fonts/IBMPlexSans-Regular.ttf"), 10)) {
    s_logger.info("Loaded IBM Plex Sans font from built-in resources");
    // also load Plex Mono for code/monospace use
    int monoId = QFontDatabase::addApplicationFont(
        QStringLiteral(":/editor/fonts/IBMPlexMono-Regular.ttf"));
    if (monoId >= 0) {
      s_logger.info("Loaded IBM Plex Mono font from built-in resources");
    }
  } else if (loadFont(QStringLiteral(":/editor/fonts/InterVariable.ttf"), 10)) {
    s_logger.info("Loaded Inter font from built-in resources");
  } else {
    s_logger.warn("No bundled font found, using system font");
  }
}

class RaidenStyle : public QProxyStyle {
public:
  int styleHint(StyleHint hint, const QStyleOption *option,
                const QWidget *widget,
                QStyleHintReturn *returnData) const override {
    if (hint == SH_UnderlineShortcut) {
      return 0;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
};

static void applyDarkPalette() {
  QApplication::setStyle(new RaidenStyle);

  QPalette pal;
  pal.setColor(QPalette::Window, QColor("#191919"));
  pal.setColor(QPalette::WindowText, QColor("#cccccc"));
  pal.setColor(QPalette::Base, QColor("#191919"));
  pal.setColor(QPalette::AlternateBase, QColor("#232323"));
  pal.setColor(QPalette::Text, QColor("#cccccc"));
  pal.setColor(QPalette::Button, QColor("#232323"));
  pal.setColor(QPalette::ButtonText, QColor("#cccccc"));
  pal.setColor(QPalette::ToolTipBase, QColor("#232323"));
  pal.setColor(QPalette::ToolTipText, QColor("#cccccc"));
  pal.setColor(QPalette::Highlight, QColor("#3D5A99"));
  pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
  pal.setColor(QPalette::PlaceholderText, QColor("#777777"));
  pal.setColor(QPalette::Disabled, QPalette::Text, QColor("#666666"));
  pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#666666"));
  pal.setColor(QPalette::Light, QColor("#2f2f2f"));
  pal.setColor(QPalette::Midlight, QColor("#2a2a2a"));
  pal.setColor(QPalette::Mid, QColor("#1a1a1a"));
  pal.setColor(QPalette::Dark, QColor("#0f0f0f"));
  pal.setColor(QPalette::Shadow, QColor("#000000"));

  QApplication::setPalette(pal);
}

static constexpr const char *kFallbackStyleSheet = R"(
  QMainWindow, QDockWidget, QMenuBar, QStatusBar, QToolBar {
    background: #191919;
    color: #cccccc;
  }
)";

static void loadStyleSheet(QApplication &qtApp,
                           const std::string &datapackPath) {
  auto fsPath = datapackPath + "/theme/styles.qss";
  QFile fsFile(QString::fromStdString(fsPath));

  if (fsFile.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream stream(&fsFile);
    qtApp.setStyleSheet(stream.readAll());
    s_logger.info("Loaded stylesheet from datapack: {}", fsPath);

    auto *watcher = new QFileSystemWatcher(&qtApp);
    watcher->addPath(QString::fromStdString(fsPath));
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged,
                     [watcher, &qtApp, fsPath](const QString &path) {
                       // re-add path in case editor replaced the file
                       watcher->addPath(path);
                       QFile f(path);
                       if (f.open(QFile::ReadOnly | QFile::Text)) {
                         QTextStream s(&f);
                         qtApp.setStyleSheet(s.readAll());
                         s_logger.info("Stylesheet reloaded: {}", fsPath);
                       }
                     });
    return;
  }

  // fallback to built-in QRC resources
  QFile file(QStringLiteral(":/editor/theme/styles.qss"));
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream stream(&file);
    qtApp.setStyleSheet(stream.readAll());
    s_logger.info("Loaded stylesheet from built-in resources");
  } else {
    s_logger.warn("styles.qss not found, using fallback theme");
    qtApp.setStyleSheet(QString::fromLatin1(kFallbackStyleSheet));
  }
}

int main(int argc, char *argv[]) {
  std::string datapackPath;
  bool standalone = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--datapack") == 0 && i + 1 < argc) {
      datapackPath = argv[++i];
    } else if (std::strcmp(argv[i], "--standalone") == 0) {
      standalone = true;
    }
  }

  QApplication qtApp(argc, argv);
  applyDarkPalette();
  loadInterFont();

  // if no --datapack, show the project launcher (unless --standalone)
  if (datapackPath.empty() && !standalone) {
    RaidenEditor::ProjectLauncherDialog launcher;
    if (launcher.exec() != QDialog::Accepted) {
      return 0;
    }
    datapackPath = launcher.selectedProjectPath();
  }

  // if still empty after launcher, run standalone with defaults
  std::filesystem::path dp;
  auto vfs = Raiden::Core::createOSFileSystem();
  bool hasDatapack = false;

  if (!datapackPath.empty()) {
    dp = std::filesystem::absolute(datapackPath);
    if (!std::filesystem::is_directory(dp)) {
      s_logger.error("Datapack directory not found: '{}'", dp.string());
      return 1;
    }
    if (!vfs->mount("game://", dp.string())) {
      s_logger.error("Failed to mount datapack.");
      return 1;
    }
    hasDatapack = true;
  } else {
    s_logger.info("No datapack, running editor standalone with defaults.");
  }

  loadStyleSheet(qtApp, hasDatapack ? dp.string() : std::string());

  auto platform = std::make_unique<Raiden::Platform::QtPlatform>();
  auto device = std::make_unique<Raiden::Renderer::VulkanDevice>();

  RaidenEditor::EditorMainWindow mainWindow(*platform, hasDatapack ? dp.string() : std::string(), *vfs);
  mainWindow.show();

  Raiden::Core::EngineConfig config;

  if (!Raiden::Core::loadConfig(*vfs, config, "Raiden Editor")) {
    s_logger.warn("Falling back to defaults.");
  }

  Raiden::Engine::Application app(std::move(platform), std::move(device),
                                  std::move(vfs));

  if (!app.init(config)) {
    return 1;
  }

  std::string pluginPath = Raiden::Core::resolvePluginPath(config.plugin);
  if (!pluginPath.empty()) {
    std::string resolvedPluginPath =
        (dp / pluginPath).lexically_normal().string();
    if (!app.loadGamePlugin(resolvedPluginPath)) {
      s_logger.warn("Game plugin not loaded, running without game.");
    } else {
      mainWindow.context()->setWorld(app.getWorld());
    }
  } else {
    s_logger.info("No game plugin configured, running without game.");
  }

  // wire gizmo renderer
  mainWindow.context()->setOverlay(app.getOverlay());
  if (mainWindow.context()->initGizmoRenderer(app.getDevice())) {
    app.setGizmoRenderCallback(
        [ctx = mainWindow.context()](::Raiden::Renderer::ICommandBuffer &cmd) {
          ctx->renderGizmoGeometry(cmd);
        });
  }

  app.run();
  return 0;
}