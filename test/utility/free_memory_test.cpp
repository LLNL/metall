// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <cstdlib>
#include <thread>
#include <chrono>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/memory.hpp>

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

namespace {
// anonymous
// map

namespace util = metall::detail::utility;
constexpr const char * k_default_test_file_path = "/tmp/free_memory_test_file";

TEST(FreeMemoryTest, PunchHoleFileSupport) {
  EXPECT_TRUE(
#if defined(FALLOC_FL_PUNCH_HOLE)
      true
#else
      false
#endif
             ) << "FALLOC_FL_PUNCH_HOLE is not defined";

  EXPECT_TRUE(
#if defined(FALLOC_FL_KEEP_SIZE)
      true
#else
      false
#endif
             ) << "FALLOC_FL_KEEP_SIZE is not defined";
}

TEST(FreeMemoryTest, MadvFreeSupport) {
  EXPECT_TRUE(
#ifdef MADV_FREE
      true
#else
      false
#endif
             ) << "MADV_FREE is not defined";
}

#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
TEST(FreeFileSpaceTest, PunchHoleFile) {

  const char *file_path = k_default_test_file_path;
  if(std::getenv("TEST_FILE_PATH")) {
    file_path = std::getenv("TEST_FILE_PATH");
  }

  ASSERT_TRUE(util::create_file(file_path));

  const ssize_t page_size = util::get_page_size();
  ASSERT_GT(page_size, 0);

  const std::size_t file_size = static_cast<std::size_t>(page_size) * 8;
  ASSERT_TRUE(util::extend_file_size(file_path, file_size));
  ASSERT_EQ(util::get_file_size(file_path), file_size);
  ASSERT_EQ(util::get_actual_file_size(file_path), 0);

  int fd;
  ASSERT_NE(fd = ::open(file_path, O_RDWR), -1);

  const std::size_t chunk_size = static_cast<std::size_t>(page_size) * 2;
  char buf[chunk_size * 2];

  ::pwrite(fd, buf, chunk_size, 0);
  ::pwrite(fd, buf, chunk_size * 2, static_cast<ssize_t>(chunk_size) * 2);
  ASSERT_EQ(::fsync(fd), 0);

  ASSERT_EQ(util::get_file_size(file_path), file_size);
  ASSERT_EQ(util::get_actual_file_size(file_path), chunk_size * 3);

  ASSERT_EQ(::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, chunk_size), 0);
  ASSERT_EQ(util::get_file_size(file_path), file_size);
  ASSERT_EQ(util::get_actual_file_size(file_path), chunk_size * 2);

  ASSERT_EQ(::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, chunk_size * 2, chunk_size * 2), 0);
  ASSERT_EQ(util::get_file_size(file_path), file_size);
  ASSERT_EQ(util::get_actual_file_size(file_path), 0);

  ::close(fd);
  ASSERT_TRUE(util::remove_file(file_path));
}
#endif

#if 0
//#if defined(RUN_LARGE_SCALE_TEST)
//TEST(FilebackedMapUncommitTest, FilebackedMapUncommitUsingMadvDontNeed) {
//  const char *file_path = k_default_test_file_path;
//  if(std::getenv("TEST_FILE_PATH")) {
//    file_path = std::getenv("TEST_FILE_PATH");
//  }
//
//  ASSERT_TRUE(util::create_file(file_path));
//
//  const ssize_t page_size = util::get_page_size();
//  ASSERT_GT(page_size, 0);
//
//  // Expect 1 GiB
//  const std::size_t file_size = static_cast<std::size_t>(page_size) * 256ULL * 1024;
//  ASSERT_TRUE(util::extend_file_size(file_path, file_size));
//
//  const int additional_flag =
//#ifdef MAP_NOSYNC
//      MAP_NOSYNC;
//#else
//      0;
//#endif
//  const auto ret = util::map_file_write_mode(file_path, nullptr, file_size, 0, additional_flag);
//  ASSERT_NE(ret.first, -1);
//  ASSERT_NE(ret.second, nullptr);
//
//  char *const map = static_cast<char *>(ret.second);
//
//  ssize_t page_cache_usage_before_commit;
//  ASSERT_GT(page_cache_usage_before_commit = util::get_page_cache_size(), 0);
//
//  // Commit pages
//  for (uint64_t i = 0; i < file_size; i += page_size) {
//    if ((i / page_size) % 2 == 0) map[i] = 1;
//  }
//  util::os_msync(map, file_size);
//  std::this_thread::sleep_for(std::chrono::seconds(1));
//
//  ssize_t page_cache_usage_after_commit;
//  ASSERT_GT(page_cache_usage_after_commit = util::get_page_cache_size(), 0);
//  EXPECT_GE(page_cache_usage_after_commit - page_cache_usage_before_commit, file_size / 2);
//
//  // Uncommit pages
//  for (uint64_t i = 0; i < file_size; i += page_size) {
//    if ((i / page_size) % 2 == 0)  {
//      ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_DONTNEED), 0) << "errno: " << errno;
//    }
//  }
//  util::os_msync(map, file_size);
//  std::this_thread::sleep_for(std::chrono::seconds(1));
//
//  ssize_t page_cache_usage_after_uncommit;
//  ASSERT_GT(page_cache_usage_after_uncommit = util::get_page_cache_size(), 0);
//  EXPECT_GE(page_cache_usage_after_commit - page_cache_usage_after_uncommit, file_size / 2);
//
//  util::munmap(ret.first, ret.second, file_size, false);
//  ASSERT_TRUE(util::remove_file(file_path));
//}
//#endif
#endif

class AnonymousMapUncommitTest : public ::testing::Test {
 protected:

  AnonymousMapUncommitTest() :
      map(nullptr),
      page_size(0),
      total_size(0),
      ram_usage_before_commit(0),
      ram_usage_after_commit(0) {}

  void SetUp() override {
    page_size = util::get_page_size();
    ASSERT_GT(page_size, 0);

    // Expect 8 GiB
    total_size = page_size * 1024ULL * 256ULL * 8ULL;
    ASSERT_NE(map = static_cast<char *>(util::map_anonymous_write_mode(nullptr, total_size, 0)),
              nullptr);

    ASSERT_GT(ram_usage_before_commit = util::get_used_ram_size(), 0);

    // Commit pages
    {
      for (uint64_t i = 0; i < total_size; i += page_size) {
        if ((i / page_size) % 2 == 0) map[i] = 1;
      }

      ASSERT_GT(ram_usage_after_commit = util::get_used_ram_size(), 0);
      EXPECT_GE(ram_usage_after_commit - ram_usage_before_commit, total_size / 2);
    }
  }

  void TearDown() override {
    util::munmap(map, total_size, false);
  }

  char *map;
  ssize_t page_size;
  std::size_t total_size;
  ssize_t ram_usage_before_commit;
  ssize_t ram_usage_after_commit;
};

#if defined(RUN_LARGE_SCALE_TEST) && defined(MADV_FREE)
TEST_F(AnonymousMapUncommitTest, UncommitAnonymousMapUsingMadvFree) {

  // Uncommit pages
  {
    for (uint64_t i = 0; i < total_size; i += page_size) {
      if ((i / page_size) % 2 == 0) {
        ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_FREE), 0) << "errno: " << errno;
      }
    }
    util::os_msync(map, total_size);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ssize_t ram_usage_after_uncommit;
    ASSERT_GT(ram_usage_after_uncommit = util::get_used_ram_size(), 0);
    EXPECT_GE(ram_usage_after_commit - ram_usage_after_uncommit, total_size / 2);
  }
}
#endif

#if defined(RUN_LARGE_SCALE_TEST)
TEST_F(AnonymousMapUncommitTest, UncommitAnonymousMapUsingMadvDontNeed) {

  // Uncommit pages
  {
    for (uint64_t i = 0; i < total_size; i += page_size) {
      if ((i / page_size) % 2 == 0) {
        ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_DONTNEED), 0) << "errno: " << errno;
      }
    }
    util::os_msync(map, total_size);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ssize_t ram_usage_after_uncommit;
    ASSERT_GT(ram_usage_after_uncommit = util::get_used_ram_size(), 0);
    EXPECT_GE(ram_usage_after_commit - ram_usage_after_uncommit, total_size / 2);
  }
}
#endif

}