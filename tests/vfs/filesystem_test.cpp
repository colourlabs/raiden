#include <gtest/gtest.h>
#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Logger.hpp>

#include <cstring>

using namespace Raiden::Core;

class VFSTest : public testing::Test {
protected:
  void SetUp() override {
    Logger::setMinLogLevel(LogLevel::Critical);
    vfs = createOSFileSystem();
  }

public:
  std::unique_ptr<IVirtualFileSystem> vfs;
};

TEST_F(VFSTest, RegisterAndReadData) {
  std::vector<std::byte> data;
  data.push_back(static_cast<std::byte>('h'));
  data.push_back(static_cast<std::byte>('i'));
  vfs->registerData("mem://test.txt", std::move(data));

  ASSERT_TRUE(vfs->exists("mem://test.txt"));

  auto file = vfs->open("mem://test.txt");
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->size(), 2);

  std::array<char, 4> buf = {};
  EXPECT_EQ(file->read(buf.data(), sizeof(buf.data()) - 1), 2);
  EXPECT_STREQ(buf.data(), "hi");
}

TEST_F(VFSTest, ReadAllFromMemory) {
  std::string content = "hello world";
  std::vector<std::byte> bytes;
  for (char c : content) {
    bytes.push_back(static_cast<std::byte>(c));
  }
  vfs->registerData("mem://greeting.txt", std::move(bytes));

  std::string result = vfs->readAll("mem://greeting.txt");
  EXPECT_EQ(result, "hello world");
}

TEST_F(VFSTest, ReadBytesFromMemory) {
  std::vector<std::byte> original = {std::byte{0x01}, std::byte{0x02}, std::byte{0xFF}};
  vfs->registerData("mem://data.bin", std::move(original));

  auto result = vfs->readBytes("mem://data.bin");
  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0], std::byte{0x01});
  EXPECT_EQ(result[2], std::byte{0xFF});
}

TEST_F(VFSTest, ExistsNonExistent) {
  EXPECT_FALSE(vfs->exists("mem://nope.txt"));
}

TEST_F(VFSTest, OpenNonExistentReturnsNull) {
  auto file = vfs->open("mem://nope.txt");
  EXPECT_EQ(file, nullptr);
}

TEST_F(VFSTest, ReadAllNonExistentReturnsEmpty) {
  std::string result = vfs->readAll("mem://nope.txt");
  EXPECT_TRUE(result.empty());
}

TEST_F(VFSTest, WriteAndReadBack) {
  EXPECT_TRUE(vfs->write("mem://written.txt", "some data"));

  ASSERT_TRUE(vfs->exists("mem://written.txt"));
  std::string result = vfs->readAll("mem://written.txt");
  EXPECT_EQ(result, "some data");
}

TEST_F(VFSTest, WriteBytesAndReadBack) {
  std::vector<std::byte> data = {std::byte{0xDE}, std::byte{0xAD}};
  EXPECT_TRUE(vfs->writeBytes("mem://written.bin", data));

  auto result = vfs->readBytes("mem://written.bin");
  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result[0], std::byte{0xDE});
  EXPECT_EQ(result[1], std::byte{0xAD});
}

TEST_F(VFSTest, MemFileSeek) {
  std::string content = "abcdefghij";
  std::vector<std::byte> bytes;
  for (char c : content) {
    bytes.push_back(static_cast<std::byte>(c));
  }
  vfs->registerData("mem://seek.txt", std::move(bytes));

  auto file = vfs->open("mem://seek.txt");
  ASSERT_NE(file, nullptr);

  std::array<char, 4> buf = {};
  file->seek(5, SEEK_SET);
  EXPECT_EQ(file->read(buf.data(), 3), 3);
  EXPECT_EQ(std::memcmp(buf.data(), "fgh", 3), 0);
}

TEST_F(VFSTest, MemFileSeekEnd) {
  std::string content = "hello";
  std::vector<std::byte> bytes;
  
  for (char c : content) {
    bytes.push_back(static_cast<std::byte>(c));
  }

  vfs->registerData("mem://seekend.txt", std::move(bytes));

  auto file = vfs->open("mem://seekend.txt");
  ASSERT_NE(file, nullptr);

  std::array<char, 4> buf = {};
  file->seek(-2, SEEK_END);
  EXPECT_EQ(file->read(buf.data(), 2), 2);
  EXPECT_EQ(std::memcmp(buf.data(), "lo", 2), 0);
}

TEST_F(VFSTest, MountDoesNotCrash) {
  // mounting a non-existent directory should return false
  EXPECT_FALSE(vfs->mount("vfs://", "/nonexistent/path"));

  // mounting with malformed paths shouldn't crash
  EXPECT_FALSE(vfs->mount("", ""));
}
