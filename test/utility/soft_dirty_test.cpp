// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <metall/detail/utility/soft_dirty_page.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/file.hpp>

namespace {

TEST(SoftDirtyTest, PageMapFile) {
  ASSERT_TRUE(metall::detail::utility::file_exist("/proc/self/pagemap"));
}

TEST(SoftDirtyTest, ResetSoftDirty) {
  ASSERT_TRUE(metall::detail::utility::reset_soft_dirty());
}

void run_in_core_test(const std::size_t num_pages, char *const map) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  const uint64_t page_no_offset = reinterpret_cast<uint64_t>(map) / page_size;
  for (uint32_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(metall::detail::utility::reset_soft_dirty());

    {
      metall::detail::utility::pagemap_reader pr;

      for (uint32_t p = 0; p < num_pages; ++p) {
        ASSERT_NE(pr.at(page_no_offset + p), pr.error_value) << "Cannot get pagemap of " << p;
        ASSERT_FALSE(metall::detail::utility::check_soft_dirty(pr.at(page_no_offset + p))) << "Not clear at " << p;
      }
    }

    for (uint32_t p = 0; p < num_pages; ++p) {
      if (p % 2 == i % 2)
        map[page_size * p] = 0;
    }

    {
      metall::detail::utility::pagemap_reader pr;
      for (uint32_t p = 0; p < num_pages; ++p) {
        ASSERT_NE(pr.at(page_no_offset + p), pr.error_value) << "Cannot get pagemap of " << p;
        if (p % 2 == i % 2)
          ASSERT_TRUE(metall::detail::utility::check_soft_dirty(pr.at(page_no_offset + p))) << "Wrong value at " << p;
        else
          ASSERT_FALSE(metall::detail::utility::check_soft_dirty(pr.at(page_no_offset + p))) << "Wrong value at " << p;
      }
    }
  }
}

TEST(SoftDirtyTest, MapAnonymous) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  char *map = static_cast<char *>(metall::detail::utility::map_anonymous_write_mode(nullptr, page_size * 4));
  ASSERT_TRUE(map);

  run_in_core_test(4, map);

  ASSERT_TRUE(metall::detail::utility::munmap(map, page_size * 4, false));
}

TEST(SoftDirtyTest, MapFileBacked) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  metall::detail::utility::create_file("/tmp/soft_dirty_test_file");
  metall::detail::utility::extend_file_size("/tmp/soft_dirty_test_file", page_size * 4);

  char *map = static_cast<char *>(metall::detail::utility::map_file_write_mode("/tmp/soft_dirty_test_file",
                                                                               nullptr,
                                                                               page_size * 4,
                                                                               0).second);
  ASSERT_TRUE(map);

  run_in_core_test(4, map);

  ASSERT_TRUE(metall::detail::utility::munmap(map, page_size * 4, false));
}
}