// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <cstdint>
#include <memory>
#include <metall/v0/kernel/chunk_directory.hpp>
#include <metall/v0/kernel/bin_number_manager.hpp>
#include <metall/metall.hpp>
#include "../test_utility.hpp"

namespace {
using chunk_no_type = metall::manager::chunk_number_type;
constexpr std::size_t k_chunk_size = metall::manager::chunk_size();

using bin_no_mngr = metall::v0::kernel::bin_number_manager<k_chunk_size, 1ULL << 48>;
constexpr int k_num_small_bins = bin_no_mngr::num_small_bins();

using directory_type = metall::v0::kernel::bin_directory<k_num_small_bins, chunk_no_type, std::allocator<char>>;
using chunk_directory_type = metall::v0::kernel::chunk_directory<chunk_no_type,
                                                                 k_chunk_size,
                                                                 1ULL << 48,
                                                                 std::allocator<char>>;

TEST(ChunkDirectoryTest, InsertSmallChunk) {
  std::allocator<char> allocator;
  chunk_directory_type directory(allocator);
  directory.allocate(k_num_small_bins);

  for (uint32_t i = 0; i < k_num_small_bins - 1; ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    ASSERT_EQ(directory.insert(bin_no), i); // It should be inserted at i-th point
  }
}

TEST(ChunkDirectoryTest, InsertLargeChunk) {
  std::allocator<char> allocator;
  chunk_directory_type directory(allocator);
  directory.allocate(1 << 20);

  std::size_t offset = 0;
  for (uint32_t i = k_num_small_bins; i < k_num_small_bins + 10; ++i) {
    ASSERT_EQ(directory.insert(i), offset);
    const std::size_t num_chunks = (bin_no_mngr::to_object_size(i) + k_chunk_size - 1) / k_chunk_size;
    offset += num_chunks;
  }
}

TEST(ChunkDirectoryTest, MarkSlot) {
  std::allocator<char> allocator;
  chunk_directory_type directory(allocator);
  directory.allocate(bin_no_mngr::num_small_bins() + 1);

  for (uint32_t i = 0; i < bin_no_mngr::num_small_bins(); ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    directory.insert(bin_no);
  }

  for (uint32_t i = 1; i < bin_no_mngr::num_small_bins(); ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    auto chunk_no = static_cast<chunk_no_type>(i);
    const std::size_t object_size = bin_no_mngr::to_object_size(bin_no);
    for (uint64_t k = 0; k < k_chunk_size / object_size; ++k) {
      ASSERT_FALSE(directory.slot_marked(chunk_no, k));
      ASSERT_FALSE(directory.all_slots_marked(chunk_no));
      ASSERT_EQ(directory.find_and_mark_slot(chunk_no), k);
      ASSERT_TRUE(directory.slot_marked(chunk_no, k));
    }
    ASSERT_TRUE(directory.all_slots_marked(chunk_no));
  }
}

TEST(ChunkDirectoryTest, UnmarkSlot) {
  std::allocator<char> allocator;
  chunk_directory_type directory(allocator);
  directory.allocate(bin_no_mngr::num_small_bins() + 1);

  for (uint32_t i = 0; i < bin_no_mngr::num_small_bins(); ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    directory.insert(bin_no);
  }

  for (uint32_t i = 0; i < bin_no_mngr::num_small_bins(); ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    auto chunk_no = static_cast<chunk_no_type>(i);
    const std::size_t object_size = bin_no_mngr::to_object_size(bin_no);
    for (uint64_t k = 0; k < k_chunk_size / object_size; ++k) {
      directory.find_and_mark_slot(chunk_no);
    }
    for (uint64_t k = 0; k < k_chunk_size / object_size; ++k) {
      ASSERT_TRUE(directory.slot_marked(chunk_no, k));
      directory.unmark_slot(chunk_no, k);
      ASSERT_FALSE(directory.slot_marked(chunk_no, k));
      ASSERT_EQ(directory.find_and_mark_slot(chunk_no), k);
    }
  }
}

TEST(ChunkDirectoryTest, Serialize) {
  std::allocator<char> allocator;
  chunk_directory_type directory(allocator);
  directory.allocate(bin_no_mngr::num_small_bins() + 4);

  for (uint32_t i = 0; i < bin_no_mngr::num_small_bins(); ++i) {
    auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
    directory.insert(bin_no);
  }
  directory.insert(bin_no_mngr::num_small_bins()); // 1 chunk
  directory.insert(bin_no_mngr::num_small_bins() + 1); // 2 chunks

  ASSERT_TRUE(metall::detail::utility::create_directory(test_utility::get_test_dir()));
  const auto file(test_utility::make_test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name()));
  ASSERT_TRUE(directory.serialize(file.c_str()));
}

TEST(ChunkDirectoryTest, Deserialize) {
  ASSERT_TRUE(metall::detail::utility::create_directory(test_utility::get_test_dir()));
  const auto file(test_utility::make_test_file_path(::testing::UnitTest::GetInstance()->current_test_info()->name()));

  {
    std::allocator<char> allocator;
    chunk_directory_type directory(allocator);
    directory.allocate(bin_no_mngr::num_small_bins() + 5);

    for (typename bin_no_mngr::bin_no_type bin_no = 0; bin_no < bin_no_mngr::num_small_bins(); ++bin_no) {
      chunk_no_type new_chunk_no = directory.insert(bin_no);
      const uint64_t num_slots = k_chunk_size / bin_no_mngr::to_object_size(bin_no);
      for (uint64_t s = 0; s < num_slots - 1; ++s) {
        directory.find_and_mark_slot(new_chunk_no);
      }
    }
    directory.insert(bin_no_mngr::num_small_bins()); // 1 chunk
    directory.insert(bin_no_mngr::num_small_bins() + 1); // 2 chunks

    directory.serialize(file.c_str());
  }

  {
    std::allocator<char> allocator;
    chunk_directory_type directory(allocator);
    directory.allocate(bin_no_mngr::num_small_bins() + 4);

    ASSERT_TRUE(directory.deserialize(file.c_str()));
    for (uint64_t i = 0; i < bin_no_mngr::num_small_bins(); ++i) {
      const auto bin_no = static_cast<typename bin_no_mngr::bin_no_type>(i);
      const auto chunk_no = static_cast<chunk_no_type>(i);

      ASSERT_EQ(directory.bin_no(chunk_no), bin_no);

      const uint64_t num_slots = k_chunk_size / bin_no_mngr::to_object_size(bin_no);
      ASSERT_EQ(directory.find_and_mark_slot(chunk_no), num_slots - 1);
    }

    const auto large_chunk1_no = static_cast<chunk_no_type>(bin_no_mngr::num_small_bins());  // use 1 chunk
    ASSERT_EQ(directory.bin_no(large_chunk1_no), bin_no_mngr::num_small_bins());
    const auto large_chunk2_no = static_cast<chunk_no_type>(bin_no_mngr::num_small_bins() + 1);
    ASSERT_EQ(directory.bin_no(large_chunk2_no), bin_no_mngr::num_small_bins() + 1); // use 2 chunks


    ASSERT_EQ(directory.insert(bin_no_mngr::num_small_bins()), large_chunk2_no + 2);
  }
}
}
