// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <metall/kernel/segment_storage/mmap_segment_storage.hpp>
#include "../test_utility.hpp"

namespace {
using segment_storage_type = metall::kernel::mmap_segment_storage<std::ptrdiff_t, std::size_t>;

const std::string &test_dir() {
  const static std::string path(test_utility::make_test_path());
  return path;
}

const std::string &test_file_prefix() {
  const static std::string path(test_dir() + "/backing_file");
  return path;
}

void prepare_test_dir() {
  ASSERT_TRUE(metall::mtlldetail::remove_file(test_dir()));
  ASSERT_TRUE(metall::mtlldetail::create_directory(test_dir()));
}

TEST(MultifileSegmentStorageTest, PageSize) {
  segment_storage_type segment_storage;
  ASSERT_GT(segment_storage.page_size(), 0);
}

TEST(MultifileSegmentStorageTest, Create) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);

  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size / 2));
    ASSERT_NE(segment_storage.get_segment(), nullptr);
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size * 2));
    ASSERT_NE(segment_storage.get_segment(), nullptr);
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));
}


TEST(MultifileSegmentStorageTest, GetSize) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);

  // vm_size < single_file_size
  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size / 2));
    ASSERT_GE(segment_storage.size(), vm_size / 2);
    ASSERT_GE(segment_storage_type::get_size(test_file_prefix()), vm_size / 2);
  }

  // vm_size > single_file_size
  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size * 2));
    ASSERT_GE(segment_storage.size(), vm_size);
    ASSERT_GE(segment_storage_type::get_size(test_file_prefix()), vm_size);
  }

  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));
}

TEST(MultifileSegmentStorageTest, Extend) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);

  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size / 2));

    // Has enough space already
    ASSERT_TRUE(segment_storage.extend(vm_size / 2));
    ASSERT_GE(segment_storage.size(), vm_size / 2);
    ASSERT_GE(segment_storage_type::get_size(test_file_prefix()), vm_size / 2);

    // Extend the space
    ASSERT_TRUE(segment_storage.extend(vm_size));
    ASSERT_GE(segment_storage.size(), vm_size);
    ASSERT_GE(segment_storage_type::get_size(test_file_prefix()), vm_size);
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));
}

TEST(MultifileSegmentStorageTest, Openable) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);
  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size));
  }
  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));

  ASSERT_TRUE(segment_storage_type::openable(test_file_prefix()));
  ASSERT_FALSE(segment_storage_type::openable(test_file_prefix() + "_dummy"));
}

TEST(MultifileSegmentStorageTest, Open) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);

  {
    prepare_test_dir();
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size));
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  // Open and Update
  {
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.open(test_file_prefix(), vm_size, addr, false));
    ASSERT_FALSE(segment_storage.read_only());
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '1');
      buf[i] = '2';
    }
  }

  // Read only
  {
    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.open(test_file_prefix(), vm_size, addr, true));
    ASSERT_TRUE(segment_storage.read_only());
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '2');
    }
  }
  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));
}

TEST(MultifileSegmentStorageTest, Sync) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  auto addr = metall::mtlldetail::reserve_vm_region(vm_size);
  ASSERT_NE(addr, nullptr);

  {
    prepare_test_dir();

    segment_storage_type segment_storage;
    ASSERT_TRUE(segment_storage.create(test_file_prefix(), vm_size, addr, vm_size / 2));
    auto buf = static_cast<char*>(segment_storage.get_segment());
    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
    segment_storage.sync(true);

    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      ASSERT_EQ(buf[i], '1');
    }

    segment_storage.extend(vm_size);
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '2';
    }
    segment_storage.sync(true);

    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '2');
    }
  }

  ASSERT_TRUE(metall::mtlldetail::munmap(addr, vm_size, true));
}
}