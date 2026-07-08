#include <Raiden/Application.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <toml++/toml.hpp>

#include <Raiden/Platform/Qt/QtPlatform.hpp>
#include <Renderer/Vulkan/VulkanDevice.hpp>

#include <RaidenEditor/EditorMainWindow.hpp>

#include <QApplication>
#include <QFile>
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

static bool loadConfig(Raiden::Core::IVirtualFileSystem &vfs,
                       Raiden::Core::EngineConfig &outConfig) {
  try {
    std::string content = vfs.readAll("game://engine.toml");
    if (content.empty()) {
      s_logger.warn("engine.toml not found in datapack.");
      return false;
    }

    auto table = toml::parse(content);

    if (auto *win = table["window"].as_table()) {
      outConfig.window.title =
          (*win)["title"].value_or(std::string("Raiden Editor"));
      outConfig.window.width = (*win)["width"].value_or(1280);
      outConfig.window.height = (*win)["height"].value_or(720);
      outConfig.window.resizable = (*win)["resizable"].value_or(true);
      outConfig.window.fullscreen = (*win)["fullscreen"].value_or(false);
      outConfig.window.vsync = (*win)["vsync"].value_or(true);
    }

    if (auto *rend = table["render"].as_table()) {
      outConfig.enableValidation = (*rend)["validation"].value_or(false);

      auto aa = (*rend)["antialiasing"].value_or(std::string("none"));
      if (aa == "msaa_x2") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx2;
      } else if (aa == "msaa_x4") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx4;
      } else if (aa == "msaa_x8") {
        outConfig.antialiasing = Raiden::Core::Antialiasing::MSAAx8;
      } else {
        outConfig.antialiasing = Raiden::Core::Antialiasing::None;
      }
    }

    return true;
  } catch (const toml::parse_error &err) {
    s_logger.error("Failed to parse engine.toml: {}", err.description());
    return false;
  }
}

static void printUsage(const char *argv0) {
  s_logger.info("Usage: {} --datapack <path>", argv0);
}

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
    if (hint == SH_UnderlineShortcut) return 0;
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

static void loadStyleSheet(QApplication &qtApp) {
  QFile file(QStringLiteral(":/editor/theme/styles.qss"));
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream stream(&file);
    qtApp.setStyleSheet(stream.readAll());
    s_logger.info("Loaded stylesheet from built-in resources");
  } else {
    s_logger.warn("styles.qss not found in resources, using fallback theme");
    qtApp.setStyleSheet(QString::fromLatin1(kFallbackStyleSheet));
  }
}

int main(int argc, char *argv[]) {
  std::string datapackPath;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--datapack") == 0 && i + 1 < argc) {
      datapackPath = argv[++i];
    }
  }

  if (datapackPath.empty()) {
    s_logger.error("No datapack specified.");
    printUsage(argv[0]);
    return 1;
  }

  std::filesystem::path dp = std::filesystem::absolute(datapackPath);
  if (!std::filesystem::is_directory(dp)) {
    s_logger.error("Datapack directory not found: '{}'", dp.string());
    return 1;
  }

  auto vfs = Raiden::Core::createOSFileSystem();
  if (!vfs->mount("game://", dp.string())) {
    s_logger.error("Failed to mount datapack.");
    return 1;
  }

  QApplication qtApp(argc, argv);
  applyDarkPalette();
  loadInterFont();
  loadStyleSheet(qtApp);

  auto platform = std::make_unique<Raiden::Platform::QtPlatform>();
  auto device = std::make_unique<Raiden::Renderer::VulkanDevice>();

  RaidenEditor::EditorMainWindow mainWindow(*platform);
  mainWindow.show();

  Raiden::Core::EngineConfig config;

  if (!loadConfig(*vfs, config)) {
    s_logger.warn("Falling back to defaults.");
  }

  Raiden::Engine::Application app(std::move(platform), std::move(device),
                                  std::move(vfs));

  if (!app.init(config)) {
    return 1;
  }

  app.run();
  return 0;
}