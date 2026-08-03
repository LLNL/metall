// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <fstream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <metall/detail/file.hpp>

#include "../test_utility.hpp"

namespace {

namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;

std::string read_file(const fs::path &path) {
  std::ifstream ifs(path);
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

bool write_string(const fs::path &path, const std::string &content) {
  std::ofstream ofs(path);
  ofs << content;
  ofs.close();
  return !!ofs;
}

fs::path prepare_test_dir(const std::string &name) {
  test_utility::create_test_dir();
  const auto dir = test_utility::make_test_path(name);
  mdtl::remove_file(dir);
  mdtl::create_directory(dir);
  return dir;
}

TEST(FileDurabilityTest, MakeNamedTempfile) {
  const auto dir = prepare_test_dir("tempfile");

  fs::path tmp_path;
  const int fd = mdtl::make_named_tempfile(dir, &tmp_path);
  ASSERT_NE(fd, -1);
  EXPECT_TRUE(mdtl::file_exist(tmp_path));
  EXPECT_EQ(tmp_path.parent_path(), dir);

  EXPECT_EQ(::write(fd, "x", 1), 1);
  EXPECT_TRUE(mdtl::os_close(fd));
  EXPECT_TRUE(mdtl::remove_file(tmp_path));
}

TEST(FileDurabilityTest, FsyncDirectory) {
  const auto dir = prepare_test_dir("fsync_dir");
  EXPECT_TRUE(mdtl::fsync_directory(dir));
  EXPECT_FALSE(mdtl::fsync_directory(dir / "does_not_exist"));
}

TEST(FileDurabilityTest, FsyncDirectoryTree) {
  const auto dir = prepare_test_dir("fsync_tree");
  ASSERT_TRUE(mdtl::create_directory(dir / "a" / "b"));
  ASSERT_TRUE(mdtl::create_file(dir / "a" / "file"));
  EXPECT_TRUE(mdtl::fsync_directory_tree(dir));
  EXPECT_FALSE(mdtl::fsync_directory_tree(dir / "does_not_exist"));
}

TEST(FileDurabilityTest, AtomicDurableReplaceFile) {
  const auto dir = prepare_test_dir("replace");

  ASSERT_TRUE(write_string(dir / "target", "old"));
  ASSERT_TRUE(write_string(dir / "replacement", "new"));

  EXPECT_TRUE(
      mdtl::atomic_durable_replace_file(dir / "target", dir / "replacement"));
  EXPECT_EQ(read_file(dir / "target"), "new");
  EXPECT_FALSE(mdtl::file_exist(dir / "replacement"));

  // The replacement must exist.
  EXPECT_FALSE(
      mdtl::atomic_durable_replace_file(dir / "target", dir / "missing"));
  EXPECT_EQ(read_file(dir / "target"), "new");
}

TEST(FileDurabilityTest, WriteFileAtomically) {
  const auto dir = prepare_test_dir("atomic_write");
  const auto target = dir / "file";

  // Initial write
  EXPECT_TRUE(mdtl::write_file_atomically(
      target,
      [](const fs::path &tmp_path) { return write_string(tmp_path, "v1"); }));
  EXPECT_EQ(read_file(target), "v1");

  // Replace
  EXPECT_TRUE(mdtl::write_file_atomically(
      target,
      [](const fs::path &tmp_path) { return write_string(tmp_path, "v2"); }));
  EXPECT_EQ(read_file(target), "v2");

  // A failed write keeps the previous content and leaves no temporary file.
  EXPECT_FALSE(mdtl::write_file_atomically(
      target, [](const fs::path &) { return false; }));
  EXPECT_EQ(read_file(target), "v2");

  std::size_t num_entries = 0;
  for ([[maybe_unused]] const auto &entry : fs::directory_iterator(dir)) {
    ++num_entries;
  }
  EXPECT_EQ(num_entries, 1);  // only the target file

  // A target in a directory that does not exist fails.
  EXPECT_FALSE(mdtl::write_file_atomically(
      dir / "no_dir" / "file",
      [](const fs::path &tmp_path) { return write_string(tmp_path, "x"); }));
}

TEST(FileDurabilityTest, ExtendFileSizeManuallyWritesZeros) {
  const auto dir = prepare_test_dir("extend");
  const auto file = dir / "file";

  const int fd = ::open(file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  ASSERT_NE(fd, -1);

  constexpr ssize_t size = 4096 * 2 + 123;  // not a multiple of the block size
  ASSERT_TRUE(mdtl::extend_file_size_manually(fd, 0, size));
  ASSERT_TRUE(mdtl::os_close(fd));

  ASSERT_EQ(mdtl::get_file_size(file), size);
  const auto content = read_file(file);
  ASSERT_EQ(content.size(), std::size_t(size));
  for (const char c : content) {
    ASSERT_EQ(c, '\0');
  }
}

}  // namespace
