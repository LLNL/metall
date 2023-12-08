// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <metall/kernel/segment_storage.hpp>
#include "../test_utility.hpp"

namespace {
using segment_storage_type = metall::kernel::segment_storage;

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
  segment_storage_type data_storage;
  ASSERT_GT(data_storage.page_size(), 0);
}

TEST(MultifileSegmentStorageTest, Create) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    ASSERT_TRUE(data_storage.is_open());
    ASSERT_TRUE(data_storage.check_sanity());
    ASSERT_NE(data_storage.get_segment(), nullptr);
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    ASSERT_TRUE(data_storage.is_open());
    ASSERT_TRUE(data_storage.check_sanity());
    ASSERT_NE(data_storage.get_segment(), nullptr);
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }
}

TEST(MultifileSegmentStorageTest, GetSize) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;

  // vm_size < single_file_size
  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    ASSERT_GE(data_storage.size(), vm_size / 2);
  }

  // vm_size > single_file_size
  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    ASSERT_GE(data_storage.size(), vm_size);
  }
}

TEST(MultifileSegmentStorageTest, Extend) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;
  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));

    // Has enough space already
    ASSERT_TRUE(data_storage.extend(vm_size / 2));
    ASSERT_GE(data_storage.size(), vm_size / 2);

    // Extend the space
    ASSERT_TRUE(data_storage.extend(vm_size));
    ASSERT_GE(data_storage.size(), vm_size);
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }
}

TEST(MultifileSegmentStorageTest, Open) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;

  {
    prepare_test_dir();
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
  }

  // Open and Update
  {
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.open(test_file_prefix(), vm_size, false));
    ASSERT_TRUE(data_storage.is_open());
    ASSERT_TRUE(data_storage.check_sanity());
    ASSERT_FALSE(data_storage.read_only());
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '1');
      buf[i] = '2';
    }
  }

  // Read only
  {
    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.open(test_file_prefix(), vm_size, true));
    ASSERT_TRUE(data_storage.is_open());
    ASSERT_TRUE(data_storage.check_sanity());
    ASSERT_TRUE(data_storage.read_only());
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '2');
    }
  }
}

TEST(MultifileSegmentStorageTest, Sync) {
  constexpr std::size_t vm_size = 1ULL << 22ULL;

  {
    prepare_test_dir();

    segment_storage_type data_storage;
    ASSERT_TRUE(data_storage.create(test_file_prefix(), vm_size));
    auto buf = static_cast<char *>(data_storage.get_segment());
    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      buf[i] = '1';
      ASSERT_EQ(buf[i], '1');
    }
    data_storage.sync(true);

    for (std::size_t i = 0; i < vm_size / 2; ++i) {
      ASSERT_EQ(buf[i], '1');
    }

    data_storage.extend(vm_size);
    for (std::size_t i = 0; i < vm_size; ++i) {
      buf[i] = '2';
    }
    data_storage.sync(true);

    for (std::size_t i = 0; i < vm_size; ++i) {
      ASSERT_EQ(buf[i], '2');
    }
  }
}
}  // namespace