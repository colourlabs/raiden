#include <Raiden/Core/ConfigLoader.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/EngineConfig.hpp>
#include <Raiden/Logger.hpp>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace Raiden::Core;

// a VFS that returns a canned string for game://engine.toml
class ConfigVFS final : public IVirtualFileSystem {
public:
  void setContent(std::string content) { content_ = std::move(content); }

  bool mount(std::string_view /*virtualPath*/, std::string_view /*realPath*/) override { return true; }
  void registerData(std::string_view /*path*/, std::vector<std::byte> /*data*/) override {}
  std::unique_ptr<IFile> open(std::string_view /*path*/) override { return nullptr; }
  [[nodiscard]] bool exists(std::string_view /*path*/) const override { return true; }
  std::string readAll(std::string_view path) override {
    if (path == "game://engine.toml") {
      return content_;
}
    return {};
  }
  std::vector<std::byte> readBytes(std::string_view /*path*/) override { return {}; }
  bool write(std::string_view /*path*/, std::string_view /*data*/) override { return true; }
  bool writeBytes(std::string_view /*path*/, const std::vector<std::byte> & /*data*/) override {
    return true;
  }

  std::string content_;
};

class ConfigLoaderTest : public ::testing::Test {
protected:
  void SetUp() override { Logger::setMinLogLevel(LogLevel::Critical); }
};

TEST_F(ConfigLoaderTest, MissingTomlReturnsFalse) {
  ConfigVFS vfs;
  EngineConfig cfg;
  EXPECT_FALSE(loadConfig(vfs, cfg));
  // defaults from EngineConfig unchanged
  EXPECT_EQ(cfg.window.width, 800);
  EXPECT_EQ(cfg.window.height, 600);
}

TEST_F(ConfigLoaderTest, EmptyTomlReturnsFalse) {
  ConfigVFS vfs;
  vfs.setContent("");
  EngineConfig cfg;
  EXPECT_FALSE(loadConfig(vfs, cfg));
}

TEST_F(ConfigLoaderTest, SetsWindowTitle) {
  ConfigVFS vfs;
  vfs.setContent(R"(
[window]
title = "My Game"
)");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_EQ(cfg.window.title, "My Game");
}

TEST_F(ConfigLoaderTest, WindowDimensions) {
  ConfigVFS vfs;
  vfs.setContent(R"(
[window]
width = 1920
height = 1080
)");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_EQ(cfg.window.width, 1920);
  EXPECT_EQ(cfg.window.height, 1080);
}

TEST_F(ConfigLoaderTest, WindowFlags) {
  ConfigVFS vfs;
  vfs.setContent(R"(
[window]
resizable = false
fullscreen = true
vsync = false
)");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_FALSE(cfg.window.resizable);
  EXPECT_TRUE(cfg.window.fullscreen);
  EXPECT_FALSE(cfg.window.vsync);
}

TEST_F(ConfigLoaderTest, DefaultTitleUsedWhenMissing) {
  ConfigVFS vfs;
  vfs.setContent("[window]\n");
  EngineConfig cfg;
  loadConfig(vfs, cfg, "FallbackTitle");
  EXPECT_EQ(cfg.window.title, "FallbackTitle");
}

TEST_F(ConfigLoaderTest, RenderValidation) {
  ConfigVFS vfs;
  vfs.setContent(R"(
[render]
validation = true
)");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_TRUE(cfg.enableValidation);
}

TEST_F(ConfigLoaderTest, AntialiasingValues) {
  ConfigVFS vfs;

  auto testAA = [&](const std::string &aaVal, Antialiasing expected) {
    vfs.setContent(std::string("[render]\nantialiasing = \"") + aaVal + "\"");
    EngineConfig cfg;
    loadConfig(vfs, cfg);
    EXPECT_EQ(cfg.antialiasing, expected);
  };

  testAA("none", Antialiasing::None);
  testAA("msaa_x2", Antialiasing::MSAAx2);
  testAA("msaa_x4", Antialiasing::MSAAx4);
  testAA("msaa_x8", Antialiasing::MSAAx8);
  testAA("bogus", Antialiasing::None); // unknown -> None
}

TEST_F(ConfigLoaderTest, PluginConfig) {
  ConfigVFS vfs;
  vfs.setContent(R"(
[game]
plugin = "fallback.so"

[game.platform]
windows = "win_plugin.dll"
macos_arm = "arm_plugin.dylib"
macos_x86 = "x86_plugin.dylib"
linux = "linux_plugin.so"
)");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_EQ(cfg.plugin.fallback, "fallback.so");
  EXPECT_EQ(cfg.plugin.windows, "win_plugin.dll");
  EXPECT_EQ(cfg.plugin.macosArm, "arm_plugin.dylib");
  EXPECT_EQ(cfg.plugin.macosX86, "x86_plugin.dylib");
  EXPECT_EQ(cfg.plugin.linux_, "linux_plugin.so");
}

TEST_F(ConfigLoaderTest, MalformedTomlReturnsFalse) {
  ConfigVFS vfs;
  // missing closing brace triggers a parse_error exception (not an assert)
  vfs.setContent("[window\ntitle = \"x\"");
  EngineConfig cfg;
  EXPECT_FALSE(loadConfig(vfs, cfg));
}

TEST_F(ConfigLoaderTest, IncompleteConfigKeepsDefaults) {
  ConfigVFS vfs;
  vfs.setContent("[window]\ntitle = \"X\"");
  EngineConfig cfg;
  loadConfig(vfs, cfg);
  EXPECT_EQ(cfg.window.title, "X");
  EXPECT_EQ(cfg.window.width, 1280); // default
  EXPECT_TRUE(cfg.window.vsync);     // default
  EXPECT_EQ(cfg.antialiasing, Antialiasing::None);
}

TEST_F(ConfigLoaderTest, ResolvePluginPathPlatform) {
  PluginConfig cfg;
  cfg.fallback = "fallback.so";
  cfg.linux_ = "linux.so";

// on Linux the result should use the linux-specific path
#ifdef __linux__
  EXPECT_EQ(resolvePluginPath(cfg), "linux.so");
#else
  EXPECT_EQ(resolvePluginPath(cfg), "fallback.so");
#endif
}

TEST_F(ConfigLoaderTest, ResolvePluginPathFallback) {
  PluginConfig cfg;
  cfg.fallback = "fallback.so";
  // no platform-specific path set
  EXPECT_EQ(resolvePluginPath(cfg), cfg.fallback);
}
