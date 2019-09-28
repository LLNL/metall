// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <metall/detail/utility/soft_dirty_page.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/memory.hpp>
#include "../test_utility.hpp"

namespace {

TEST(SoftDirtyTest, PageMapFile) {
  ASSERT_TRUE(metall::detail::utility::file_exist("/proc/self/pagemap"));
}

TEST(SoftDirtyTest, ResetSoftDirty) {
  ASSERT_TRUE(metall::detail::utility::reset_soft_dirty_bit());
}

void run_in_core_test(const std::size_t num_pages, char *const map) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  const uint64_t page_no_offset = reinterpret_cast<uint64_t>(map) / page_size;
  for (uint64_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(metall::detail::utility::reset_soft_dirty_bit());

    {
      metall::detail::utility::pagemap_reader pr;

      for (uint64_t p = 0; p < num_pages; ++p) {
        ASSERT_NE(pr.at(page_no_offset + p), pr.error_value) << "page no " << p;
        ASSERT_FALSE(metall::detail::utility::check_soft_dirty_page(pr.at(page_no_offset + p))) << "page no " << p;
      }
    }

    for (uint64_t p = 0; p < num_pages; ++p) {
      if (p % 2 == i % 2)
        map[page_size * p] = 0;
    }
    // metall::detail::utility::os_msync(map, page_size * num_pages);

    {
      metall::detail::utility::pagemap_reader pr;
      for (uint64_t p = 0; p < num_pages; ++p) {
        [[maybe_unused]] volatile int c = map[p * page_size];
        const auto pagemap_value = pr.at(page_no_offset + p);
        ASSERT_NE(pagemap_value, pr.error_value) << "Cannot read pagemap at " << p;
        if (p % 2 == i % 2)
          ASSERT_TRUE(metall::detail::utility::check_soft_dirty_page(pagemap_value))
                        << "page no " << p << ", pagemap value " << pagemap_value;
        else
          ASSERT_FALSE(metall::detail::utility::check_soft_dirty_page(pagemap_value))
                        << "page no " << p << ", pagemap value " << pagemap_value;
      }
    }
  }
}

TEST(SoftDirtyTest, MapAnonymous) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  const std::size_t k_num_pages = 4;

  char *map = static_cast<char *>(metall::detail::utility::map_anonymous_write_mode(nullptr, page_size * k_num_pages));
  ASSERT_TRUE(map);

  run_in_core_test(k_num_pages, map);

  ASSERT_TRUE(metall::detail::utility::munmap(map, page_size * k_num_pages, false));
}

TEST(SoftDirtyTest, MapFileBacked) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  ASSERT_TRUE(test_utility::create_test_dir());
  const auto file(test_utility::make_test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name()));
  const std::size_t k_num_pages = 4;

  metall::detail::utility::create_file(file.c_str());
  metall::detail::utility::extend_file_size(file.c_str(), page_size * k_num_pages);

  char *map = static_cast<char *>(metall::detail::utility::map_file_write_mode(file.c_str(),
                                                                               nullptr,
                                                                               page_size * k_num_pages,
                                                                               0).second);
  ASSERT_TRUE(map);

  run_in_core_test(k_num_pages, map);

  ASSERT_TRUE(metall::detail::utility::munmap(map, page_size * k_num_pages, false));
}

TEST(SoftDirtyTest, MapPrivateFileBacked) {
  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  ASSERT_TRUE(test_utility::create_test_dir());
  const auto file(test_utility::make_test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name()));
  const std::size_t k_num_pages = 4;

  metall::detail::utility::create_file(file.c_str());
  metall::detail::utility::extend_file_size(file.c_str(), page_size * k_num_pages);

  const int fd = ::open(file.c_str(), O_RDWR);
  ASSERT_NE(fd, -1);

  auto map = reinterpret_cast<char *>(metall::detail::utility::os_mmap(nullptr,
                                                                       page_size * k_num_pages,
                                                                       PROT_READ | PROT_WRITE,
                                                                       MAP_PRIVATE,
                                                                       fd,
                                                                       0));
  ASSERT_TRUE(map);

  run_in_core_test(k_num_pages, map);

  ASSERT_TRUE(metall::detail::utility::munmap(map, page_size * k_num_pages, false));
}
}