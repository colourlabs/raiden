#include <gtest/gtest.h>
#include <Raiden/Assets/AssetManager.hpp>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Jobs/JobSystem.hpp>
#include <Raiden/Logger.hpp>

#include "NullRenderDevice.hpp"

#include <memory>

using namespace Raiden::Assets;
using namespace Raiden::Core;
using namespace Raiden::Jobs;
using namespace Raiden::Renderer;

// helper VFS that holds in-memory data

class TestVFS final : public IVirtualFileSystem {
public:
  bool mount(std::string_view /*virtualPath*/, std::string_view /*realPath*/) override { return true; }
  void registerData(std::string_view path, std::vector<std::byte> data) override {
    store_[std::string(path)] = std::move(data);
  }
  std::unique_ptr<IFile> open(std::string_view /*path*/) override { return nullptr; }
  bool exists(std::string_view /*path*/) const override { return true; }
  std::string readAll(std::string_view path) override {
    auto it = store_.find(std::string(path));
    if (it == store_.end()) { return {};
}
    return {reinterpret_cast<const char *>(it->second.data()), it->second.size()};
  }
  std::vector<std::byte> readBytes(std::string_view path) override {
    auto it = store_.find(std::string(path));
    if (it == store_.end()) { return {};
}
    return it->second;
  }
  bool write(std::string_view /*path*/, std::string_view /*data*/) override { return true; }
  bool writeBytes(std::string_view /*path*/, const std::vector<std::byte> & /*data*/) override { return true; }

  std::unordered_map<std::string, std::vector<std::byte>> store_;
};

// tests

class AssetManagerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Logger::setMinLogLevel(LogLevel::Critical);
  }
};

TEST_F(AssetManagerTest, MissingFileSyncReturnsNull) {
  NullRenderDevice device;
  TestVFS vfs;
  AssetManager am(device, vfs);
  EXPECT_EQ(am.loadTextureSync("nonexistent.ktx2"), nullptr);
}

TEST_F(AssetManagerTest, MissingFileAsyncReturnsNull) {
  NullRenderDevice device;
  TestVFS vfs;
  AssetManager am(device, vfs);
  // No JobSystem set, falls back to sync which returns null for missing file
  EXPECT_EQ(am.loadTexture("nonexistent.ktx2"), nullptr);
}

TEST_F(AssetManagerTest, AsyncTextureFallbackToSyncWhenNoJobSystem) {
  NullRenderDevice device;
  TestVFS vfs;
  vfs.store_["tex/test.ktx2"] = {}; // empty bytes -> decode fails -> returns null

  AssetManager am(device, vfs);
  auto tex = am.loadTexture("tex/test.ktx2");
  EXPECT_EQ(tex, nullptr);
}

TEST_F(AssetManagerTest, SyncTextureMissingFileReturnsNull) {
  NullRenderDevice device;
  TestVFS vfs;
  AssetManager am(device, vfs);
  EXPECT_EQ(am.loadTextureSync("tex/missing.png"), nullptr);
}

TEST_F(AssetManagerTest, LoadMaterial) {
  NullRenderDevice device;
  TestVFS vfs;

  AssetManager am(device, vfs);
  MaterialDesc desc;
  desc.shader = "builtin://pbr";

  auto mat = am.loadMaterial(desc);
  ASSERT_NE(mat, nullptr);
  ASSERT_EQ(device.materialDescs_.size(), 1U);
  EXPECT_EQ(device.materialDescs_[0].shader, "builtin://pbr");
}

TEST_F(AssetManagerTest, LoadMaterialCaches) {
  NullRenderDevice device;
  TestVFS vfs;

  AssetManager am(device, vfs);
  MaterialDesc desc;
  desc.shader = "builtin://pbr";

  auto m1 = am.loadMaterial(desc);
  auto m2 = am.loadMaterial(desc);
  EXPECT_EQ(m1, m2);
  EXPECT_EQ(device.materialDescs_.size(), 1U);
}

TEST_F(AssetManagerTest, LoadMaterialWithTextures) {
  NullRenderDevice device;
  TestVFS vfs;

  AssetManager am(device, vfs);
  MaterialDesc desc;
  desc.shader = "builtin://pbr";
  desc.baseColorTexture = "tex/albedo.png";
  // VFS will return empty bytes -> texture load returns null -> material still created with null texture

  auto mat = am.loadMaterial(desc);
  ASSERT_NE(mat, nullptr);
  EXPECT_EQ(device.materialDescs_.size(), 1U);
}

TEST_F(AssetManagerTest, LoadMeshWithUnsupportedFormat) {
  NullRenderDevice device;
  TestVFS vfs;
  vfs.store_["mesh/bad.txt"] = {std::byte{'d'}, std::byte{'a'}, std::byte{'t'}, std::byte{'a'}};

  AssetManager am(device, vfs);
  auto model = am.loadMesh("mesh/bad.txt");
  EXPECT_EQ(model, nullptr);
}

TEST_F(AssetManagerTest, LoadMeshFileNotFound) {
  NullRenderDevice device;
  TestVFS vfs;

  AssetManager am(device, vfs);
  auto model = am.loadMesh("mesh/nonexistent.glb");
  EXPECT_EQ(model, nullptr);
}

TEST_F(AssetManagerTest, ReleaseUnusedNoCrashWhenEmpty) {
  NullRenderDevice device;
  TestVFS vfs;
  AssetManager am(device, vfs);
  am.releaseUnused(); // should not crash
}

TEST_F(AssetManagerTest, ProcessLoadQueueNoOpsWhenNothingPending) {
  NullRenderDevice device;
  TestVFS vfs;
  AssetManager am(device, vfs);
  am.processLoadQueue(); // should not crash
}

TEST_F(AssetManagerTest, DuplicatePendingTextureReturnsNull) {
  NullRenderDevice device;
  TestVFS vfs;
  vfs.store_["tex/t.png"] = {};

  JobSystem js;
  js.init({.numWorkers = 1});

  AssetManager am(device, vfs);
  am.setJobSystem(js);

  EXPECT_EQ(am.loadTexture("tex/t.png"), nullptr);  // pending
  EXPECT_EQ(am.loadTexture("tex/t.png"), nullptr);  // already pending -> null
}

TEST_F(AssetManagerTest, RegisterDataThenQueryCachedCount) {
  NullRenderDevice device;
  TestVFS vfs;

  AssetManager am(device, vfs);
  EXPECT_EQ(am.cachedTextureCount(), 0U);
  EXPECT_EQ(am.cachedMaterialCount(), 0U);
  EXPECT_EQ(am.cachedMeshCount(), 0U);
}
