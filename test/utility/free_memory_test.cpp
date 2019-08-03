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

#include "../test_utility.hpp"

namespace {

namespace util = metall::detail::utility;

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

TEST(FreeMemoryTest, MadvRemoveSupport) {
  EXPECT_TRUE(
#ifdef MADV_REMOVE
      true
#else
      false
#endif
             ) << "MADV_REMOVE is not defined";
}

#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
TEST(FreeFileSpaceTest, PunchHoleFile) {

  const auto file = test_utility::test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name());;

  ASSERT_TRUE(util::create_file(file.c_str()));

  const ssize_t page_size = util::get_page_size();
  ASSERT_GT(page_size, 0);

  const std::size_t file_size = static_cast<std::size_t>(page_size) * 8;
  ASSERT_TRUE(util::extend_file_size(file.c_str(), file_size));
  ASSERT_EQ(util::get_file_size(file.c_str()), file_size);
  ASSERT_GE(util::get_actual_file_size(file.c_str()), 0);

  int fd;
  ASSERT_NE(fd = ::open(file.c_str(), O_RDWR), -1);

  const std::size_t chunk_size = static_cast<std::size_t>(page_size) * 2;
  char buf[chunk_size * 2];

  ::pwrite(fd, buf, chunk_size, 0);
  ::pwrite(fd, buf, chunk_size * 2, static_cast<ssize_t>(chunk_size) * 2);
  ASSERT_EQ(::fsync(fd), 0);

  ASSERT_EQ(util::get_file_size(file.c_str()), file_size);
  ASSERT_GE(util::get_actual_file_size(file.c_str()), chunk_size * 3);

  ASSERT_EQ(::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, chunk_size), 0);
  ASSERT_EQ(util::get_file_size(file.c_str()), file_size);
  ASSERT_GE(util::get_actual_file_size(file.c_str()), chunk_size * 2);

  ASSERT_EQ(::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, chunk_size * 2, chunk_size * 2), 0);
  ASSERT_EQ(util::get_file_size(file.c_str()), file_size);
  ASSERT_GE(util::get_actual_file_size(file.c_str()), 0);

  ::close(fd);
  ASSERT_TRUE(util::remove_file(file.c_str()));
}
#endif

class AnonymousMapUncommitTest : public ::testing::Test {
 protected:

  AnonymousMapUncommitTest() :
      map(nullptr),
      page_size(0),
      map_size(0),
      committed_size(0),
      ram_usage_before_commit(0),
      ram_usage_after_commit(0) {}

  void SetUp() override {
    page_size = util::get_page_size();
    ASSERT_GT(page_size, 0);

    // Expect 8 GiB
    map_size = page_size * 1024ULL * 256ULL * 8ULL;
    ASSERT_NE(map = static_cast<char *>(util::map_anonymous_write_mode(nullptr, map_size, 0)),
              nullptr);

    ASSERT_GT(ram_usage_before_commit = util::get_used_ram_size(), 0);

    // Commit pages
    {
      for (uint64_t i = 0; i < map_size; i += page_size) {
        if ((i / page_size) % 2 == 0) {
          map[i] = 1;
          committed_size += page_size;
        }
      }

      ASSERT_GT(ram_usage_after_commit = util::get_used_ram_size(), 0);
      EXPECT_GE(ram_usage_after_commit - ram_usage_before_commit, committed_size);
    }
  }

  void TearDown() override {
    util::munmap(map, map_size, false);
  }

  char *map;
  ssize_t page_size;
  std::size_t map_size;
  std::size_t committed_size;
  ssize_t ram_usage_before_commit;
  ssize_t ram_usage_after_commit;
};

#if defined(METALL_RUN_LARGE_SCALE_TEST) && defined(MADV_FREE)
TEST_F(AnonymousMapUncommitTest, MadvFree) {

  // Uncommit pages
  {
    for (uint64_t i = 0; i < map_size; i += page_size) {
      if ((i / page_size) % 2 == 0) {
        ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_FREE), 0) << "errno: " << errno;
      }
    }
    util::os_msync(map, map_size, true);

    ssize_t ram_usage_after_uncommit;
    ASSERT_GT(ram_usage_after_uncommit = util::get_used_ram_size(), 0);
    EXPECT_GE(ram_usage_after_commit - ram_usage_after_uncommit, committed_size);
  }
}
#endif

#if defined(METALL_RUN_LARGE_SCALE_TEST)
TEST_F(AnonymousMapUncommitTest, MadvDontNeed) {

  // Uncommit pages
  {
    // Commit only half pages
    for (uint64_t i = 0; i < map_size; i += page_size) {
      if ((i / page_size) % 2 == 0) {
        ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_DONTNEED), 0) << "errno: " << errno;
      }
    }
    util::os_msync(map, map_size, true);

    ssize_t ram_usage_after_uncommit;
    ASSERT_GT(ram_usage_after_uncommit = util::get_used_ram_size(), 0);
    EXPECT_GE(ram_usage_after_commit - ram_usage_after_uncommit, committed_size);
  }
}
#endif

class FilebackedMapUncommitTest : public ::testing::Test {
 protected:

  FilebackedMapUncommitTest() :
      map(nullptr),
      page_size(0),
      file_name(),
      file_size(0),
      committed_size(0),
      page_cache_usage_before_commit(0),
      page_cache_usage_after_commit(0) {}

  void SetUp() override {
    file_name = test_utility::test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name());;

    ASSERT_TRUE(util::create_file(file_name.c_str()));

    page_size = util::get_page_size();
    ASSERT_GT(page_size, 0);
    // Expect 8 GiB
    file_size = page_size * 1024ULL * 256ULL * 8ULL;

    ASSERT_TRUE(util::extend_file_size(file_name.c_str(), file_size));
    ASSERT_EQ(util::get_file_size(file_name.c_str()), file_size);
    ASSERT_GE(util::get_actual_file_size(file_name.c_str()), 0);

    int fd = -1;
    void *addr = nullptr;
    std::tie(fd, addr) = util::map_file_write_mode(file_name, nullptr, file_size, 0);
    ASSERT_NE(fd, -1);
    ASSERT_NE(addr, nullptr);
    map = static_cast<char*>(addr);
    ::close(fd);
    
    ASSERT_GE(page_cache_usage_before_commit = util::get_page_cache_size(), 0);

    // Commit pages
    {
      // Commit only half pages
      for (uint64_t i = 0; i < file_size; i += page_size) {
        if ((i / page_size) % 2 == 0) {
          map[i] = 1;
          committed_size += page_size;
        }
      }
      util::os_msync(addr, file_size, true);

      ASSERT_EQ(util::get_file_size(file_name.c_str()), file_size);
      ASSERT_GE(util::get_actual_file_size(file_name.c_str()), committed_size);

      ASSERT_GT(page_cache_usage_after_commit = util::get_page_cache_size(), 0);
      EXPECT_GE(page_cache_usage_after_commit - page_cache_usage_before_commit, committed_size);
    }
  }

  void TearDown() override {
    util::munmap(map, file_size, false);
  }

  char *map;
  ssize_t page_size;
  std::string file_name;
  std::size_t file_size;
  std::size_t committed_size;
  ssize_t page_cache_usage_before_commit;
  ssize_t page_cache_usage_after_commit;
};

#if defined(METALL_RUN_LARGE_SCALE_TEST) && defined(MADV_REMOVE)
TEST_F(FilebackedMapUncommitTest, MadvRemove) {

  // Uncommit pages
  {
    for (uint64_t i = 0; i < file_size; i += page_size) {
      if ((i / page_size) % 2 == 0) {
        ASSERT_EQ(::madvise(&map[i], static_cast<std::size_t>(page_size), MADV_REMOVE), 0) << "errno: " << errno;
      }
    }
    util::os_msync(map, file_size, true);

    ASSERT_EQ(util::get_file_size(file_name.c_str()), file_size);
    ASSERT_GE(util::get_actual_file_size(file_name.c_str()), 0);

    ssize_t page_cache_usage_after_uncommit;
    ASSERT_GE(page_cache_usage_after_uncommit = util::get_page_cache_size(), 0);
    EXPECT_GE(page_cache_usage_after_commit - page_cache_usage_after_uncommit, committed_size);
  }
}
#endif
}