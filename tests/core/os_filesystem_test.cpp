#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

using namespace Raiden::Core;

class OSFileSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Logger::setMinLogLevel(LogLevel::Critical);
    tmpDir = std::filesystem::temp_directory_path() / "raiden_vfs_test_XXXXXX";

    // create a unique temp directory
    std::array<char, 256> tmpl;
    std::snprintf(tmpl.data(), tmpl.size(), "%s/raiden_vfs_test_XXXXXX",
                  std::filesystem::temp_directory_path().c_str());

    char *result = ::mkdtemp(tmpl.data());
    ASSERT_NE(result, nullptr) << "Failed to create temp dir";
    tmpDir = result;

    vfs = createOSFileSystem();
    ASSERT_TRUE(vfs->mount("game://", tmpDir.string()));
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);
  }

public:
  std::unique_ptr<IVirtualFileSystem> vfs;
  std::filesystem::path tmpDir;
};

TEST_F(OSFileSystemTest, MountNonExistentDirReturnsFalse) {
  // Already mounted in SetUp; try a bogus one
  auto vfs2 = createOSFileSystem();
  EXPECT_FALSE(vfs2->mount("bad://", "/nonexistent_dir_12345"));
}

TEST_F(OSFileSystemTest, MountEmptyPathNoCrash) {
  auto vfs2 = createOSFileSystem();
  EXPECT_FALSE(vfs2->mount("", ""));
}

TEST_F(OSFileSystemTest, WriteAndReadBackFile) {
  EXPECT_TRUE(vfs->write("game://hello.txt", "Hello, World!"));
  EXPECT_TRUE(vfs->exists("game://hello.txt"));
  auto content = vfs->readAll("game://hello.txt");
  EXPECT_EQ(content, "Hello, World!");
}

TEST_F(OSFileSystemTest, WriteBytesAndReadBack) {
  std::vector<std::byte> data = {std::byte{0xDE}, std::byte{0xAD},
                                 std::byte{0xBE}, std::byte{0xEF}};
  EXPECT_TRUE(vfs->writeBytes("game://data.bin", data));

  auto result = vfs->readBytes("game://data.bin");
  ASSERT_EQ(result.size(), 4);
  EXPECT_EQ(result[0], std::byte{0xDE});
  EXPECT_EQ(result[3], std::byte{0xEF});
}

TEST_F(OSFileSystemTest, OpenRealFileAndSeek) {
  ASSERT_TRUE(vfs->write("game://seek.txt", "0123456789"));

  auto file = vfs->open("game://seek.txt");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->size(), 10);

  std::array<char, 4> buf = {};
  file->seek(3, SEEK_SET);
  EXPECT_EQ(file->read(buf.data(), 3), 3);
  EXPECT_STREQ(buf.data(), "345");

  file->seek(-2, SEEK_END);
  EXPECT_EQ(file->read(buf.data(), 2), 2);
  EXPECT_EQ(std::memcmp(buf.data(), "89", 2), 0);
}

TEST_F(OSFileSystemTest, OpenNonexistentFileReturnsNull) {
  auto file = vfs->open("game://no_such_file.txt");
  EXPECT_EQ(file, nullptr);
}

TEST_F(OSFileSystemTest, ReadAllNonexistentReturnsEmpty) {
  EXPECT_TRUE(vfs->readAll("game://nope.txt").empty());
}

TEST_F(OSFileSystemTest, WriteCreatesIntermediateDirectories) {
  EXPECT_TRUE(vfs->write("game://sub/dir/test.txt", "nested"));
  EXPECT_TRUE(vfs->exists("game://sub/dir/test.txt"));
  EXPECT_EQ(vfs->readAll("game://sub/dir/test.txt"), "nested");
}

TEST_F(OSFileSystemTest, OverwriteExistingFile) {
  EXPECT_TRUE(vfs->write("game://overwrite.txt", "old"));
  EXPECT_TRUE(vfs->write("game://overwrite.txt", "new"));
  EXPECT_EQ(vfs->readAll("game://overwrite.txt"), "new");
}

TEST_F(OSFileSystemTest, MemFileTakesPriorityOverMounted) {
  // register in-memory data at a path that would also resolve via mount
  std::vector<std::byte> memData;
  const auto *s = "MEMORY";

  memData.reserve(std::strlen(s));
  for (size_t i = 0; i < std::strlen(s); i++) {
    memData.push_back(static_cast<std::byte>(s[i]));
  }

  vfs->registerData("game://conflict.txt", std::move(memData));

  // Also write a file to the real path
  EXPECT_TRUE(vfs->write("game://conflict.txt", "DISK"));

  // read should return the in-memory version
  EXPECT_EQ(vfs->readAll("game://conflict.txt"), "MEMORY");
}

TEST_F(OSFileSystemTest, WriteBytesRoundTrip) {
  std::vector<std::byte> original = {std::byte{0x00}, std::byte{0xFF},
                                     std::byte{0x7F}, std::byte{0x80}};
  EXPECT_TRUE(vfs->writeBytes("game://roundtrip.bin", original));
  auto result = vfs->readBytes("game://roundtrip.bin");
  EXPECT_EQ(original, result);
}

TEST_F(OSFileSystemTest, ExistsForMemData) {
  vfs->registerData("mem://present.txt", {std::byte{'a'}});
  EXPECT_TRUE(vfs->exists("mem://present.txt"));
  EXPECT_FALSE(vfs->exists("mem://absent.txt"));
}
