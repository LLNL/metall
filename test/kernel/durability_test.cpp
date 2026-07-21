// Copyright 2026 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <filesystem>

#include <unistd.h>

#include <metall/metall.hpp>

#include "../sandbox.hpp"
#include "../test_utility.hpp"

namespace {

namespace fs = std::filesystem;
using test_utility::subprocess_result;

// Internal datastore layout, used to verify on-disk state.
constexpr const char *k_root_dir_name = "mds";
constexpr const char *k_lock_file_name = "mds_lock";
constexpr const char *k_tmp_datastore_dir_name = ".tmp_datastore";
constexpr const char *k_mark_file_name = "properly_closed_mark";

fs::path ds_path(const std::string &name) {
  test_utility::create_test_dir();
  return test_utility::make_test_path(name);
}

// A destructed manager closes the datastore properly. The next process sees
// a consistent datastore with the data.
TEST(DurabilityTest, ProperCloseAcrossProcesses) {
  const auto path = ds_path("proper_close");
  metall::manager::remove(path);

  const auto res = METALL_SANDBOX {
    metall::manager manager(metall::create_only, path);
    auto *v = manager.construct<int>("v")(42);
    if (!v) return 1;
    return 0;
    // The manager destructor runs before the subprocess exits.
  };
  ASSERT_EQ(res, subprocess_result::exit_success);

  ASSERT_TRUE(metall::manager::consistent(path));
  ASSERT_TRUE(fs::exists(path / k_root_dir_name / k_mark_file_name));

  metall::manager manager(metall::open_read_only, path);
  ASSERT_TRUE(manager.check_sanity());
  const auto v = manager.find<int>("v");
  ASSERT_NE(v.first, nullptr);
  EXPECT_EQ(*v.first, 42);
}

// A process that dies while the datastore is open leaves no properly-closed
// mark. The datastore is reported as inconsistent and cannot be opened.
TEST(DurabilityTest, CrashWhileOpenIsDetected) {
  const auto path = ds_path("crash_open");
  metall::manager::remove(path);

  const auto res = METALL_SANDBOX {
    metall::manager manager(metall::create_only, path);
    manager.construct<int>("v")(42);
    ::_exit(0);  // dies without closing: destructors do not run
  };
  ASSERT_EQ(res, subprocess_result::exit_success);

  EXPECT_FALSE(metall::manager::consistent(path));
  EXPECT_FALSE(fs::exists(path / k_root_dir_name / k_mark_file_name));

  {
    metall::manager manager(metall::open_only, path);
    EXPECT_FALSE(manager.check_sanity());
  }
  {
    metall::manager manager(metall::open_read_only, path);
    EXPECT_FALSE(manager.check_sanity());
  }
}

// A close that cannot persist the management data does not create the
// properly-closed mark.
TEST(DurabilityTest, FailedCloseLeavesNoMark) {
  if (::geteuid() == 0) {
    GTEST_SKIP() << "Runs as root; permissions do not restrict writes";
  }

  const auto path = ds_path("failed_close");
  metall::manager::remove(path);

  const auto management_dir = path / k_root_dir_name / "management";
  {
    metall::manager manager(metall::create_only, path);
    ASSERT_TRUE(manager.check_sanity());
    manager.construct<int>("v")(42);

    // Writing the management data fails at close.
    fs::permissions(management_dir,
                    fs::perms::owner_read | fs::perms::owner_exec);
  }
  fs::permissions(management_dir, fs::perms::owner_all);

  EXPECT_FALSE(metall::manager::consistent(path));
  EXPECT_FALSE(fs::exists(path / k_root_dir_name / k_mark_file_name));
}

// One writer excludes all other opens. Multiple readers share.
TEST(DurabilityTest, OpenLockMatrix) {
  const auto path = ds_path("lock_matrix");
  metall::manager::remove(path);

  {
    metall::manager manager(metall::create_only, path);
    ASSERT_TRUE(manager.check_sanity());
    manager.construct<int>("v")(1);

    metall::manager second_writer(metall::open_only, path);
    EXPECT_FALSE(second_writer.check_sanity());
    metall::manager reader(metall::open_read_only, path);
    EXPECT_FALSE(reader.check_sanity());
  }

  {
    metall::manager reader1(metall::open_read_only, path);
    ASSERT_TRUE(reader1.check_sanity());
    metall::manager reader2(metall::open_read_only, path);
    EXPECT_TRUE(reader2.check_sanity());

    // The mark still exists while readers are open, so this failure comes
    // from the lock, not from a missing mark.
    ASSERT_TRUE(fs::exists(path / k_root_dir_name / k_mark_file_name));
    metall::manager writer(metall::open_only, path);
    EXPECT_FALSE(writer.check_sanity());
  }

  // After all managers are closed the datastore opens normally.
  metall::manager writer(metall::open_only, path);
  ASSERT_TRUE(writer.check_sanity());
  const auto v = writer.find<int>("v");
  ASSERT_NE(v.first, nullptr);
  EXPECT_EQ(*v.first, 1);
}

// Re-creating a datastore that is open elsewhere is refused and does not
// destroy the data.
TEST(DurabilityTest, CreateRefusedWhileOpen) {
  const auto path = ds_path("create_while_open");
  metall::manager::remove(path);

  {
    metall::manager manager(metall::create_only, path);
    ASSERT_TRUE(manager.check_sanity());
    manager.construct<int>("v")(7);

    metall::manager creator(metall::create_only, path);
    EXPECT_FALSE(creator.check_sanity());

    // The open manager is still usable.
    const auto v = manager.find<int>("v");
    ASSERT_NE(v.first, nullptr);
    EXPECT_EQ(*v.first, 7);
  }

  // The data survived the refused re-creation.
  ASSERT_TRUE(metall::manager::consistent(path));
  metall::manager manager(metall::open_read_only, path);
  const auto v = manager.find<int>("v");
  ASSERT_NE(v.first, nullptr);
  EXPECT_EQ(*v.first, 7);
}

// Removing a datastore that is open elsewhere is refused.
TEST(DurabilityTest, RemoveRefusedWhileOpen) {
  const auto path = ds_path("remove_while_open");
  metall::manager::remove(path);

  {
    metall::manager manager(metall::create_only, path);
    ASSERT_TRUE(manager.check_sanity());
    manager.construct<int>("v")(7);

    EXPECT_FALSE(metall::manager::remove(path));

    const auto v = manager.find<int>("v");
    ASSERT_NE(v.first, nullptr);
  }

  EXPECT_TRUE(metall::manager::remove(path));
  EXPECT_FALSE(metall::manager::consistent(path));
}

// The lockfile is created once at datastore creation. A datastore without a
// lockfile is broken and refuses to open. Re-creation makes it usable again.
TEST(DurabilityTest, MissingLockfileRefusesOpen) {
  const auto path = ds_path("no_lockfile");
  metall::manager::remove(path);

  {
    metall::manager manager(metall::create_only, path);
    ASSERT_TRUE(manager.check_sanity());
    manager.construct<int>("v")(9);
  }

  ASSERT_TRUE(fs::exists(path / k_lock_file_name));
  ASSERT_TRUE(fs::remove(path / k_lock_file_name));

  {
    metall::manager manager(metall::open_only, path);
    EXPECT_FALSE(manager.check_sanity());
  }
  {
    metall::manager manager(metall::open_read_only, path);
    EXPECT_FALSE(manager.check_sanity());
  }

  // create() recreates the lockfile and the datastore.
  {
    metall::manager manager(metall::create_only, path);
    EXPECT_TRUE(manager.check_sanity());
  }
  EXPECT_TRUE(fs::exists(path / k_lock_file_name));
}

// A snapshot is complete under its final path and leaves no temporary
// directory behind.
TEST(DurabilityTest, SnapshotIsPublishedAtomically) {
  const auto src = ds_path("snapshot_src");
  const auto dst = ds_path("snapshot_dst");
  metall::manager::remove(src);
  metall::manager::remove(dst);

  {
    metall::manager manager(metall::create_only, src);
    manager.construct<int>("v")(11);
    ASSERT_TRUE(manager.snapshot(dst));

    // A second snapshot replaces the destination.
    *(manager.find<int>("v").first) = 12;
    ASSERT_TRUE(manager.snapshot(dst));
  }

  EXPECT_TRUE(metall::manager::consistent(dst));
  EXPECT_FALSE(fs::exists(dst / k_tmp_datastore_dir_name));

  metall::manager manager(metall::open_read_only, dst);
  const auto v = manager.find<int>("v");
  ASSERT_NE(v.first, nullptr);
  EXPECT_EQ(*v.first, 12);
}

// A snapshot onto a datastore that is open elsewhere is refused.
TEST(DurabilityTest, SnapshotRefusedOntoOpenDatastore) {
  const auto src = ds_path("snapshot_busy_src");
  const auto dst = ds_path("snapshot_busy_dst");
  metall::manager::remove(src);
  metall::manager::remove(dst);

  metall::manager source_manager(metall::create_only, src);
  source_manager.construct<int>("v")(1);

  metall::manager destination_manager(metall::create_only, dst);
  ASSERT_TRUE(destination_manager.check_sanity());

  EXPECT_FALSE(source_manager.snapshot(dst));

  // The open destination is not damaged.
  EXPECT_TRUE(destination_manager.check_sanity());
}

// A copy is complete under its final path and leaves no temporary directory
// behind.
TEST(DurabilityTest, CopyIsPublishedAtomically) {
  const auto src = ds_path("copy_src");
  const auto dst = ds_path("copy_dst");
  metall::manager::remove(src);
  metall::manager::remove(dst);

  {
    metall::manager manager(metall::create_only, src);
    manager.construct<int>("v")(21);
  }

  ASSERT_TRUE(metall::manager::copy(src, dst));
  EXPECT_TRUE(metall::manager::consistent(dst));
  EXPECT_FALSE(fs::exists(dst / k_tmp_datastore_dir_name));

  metall::manager manager(metall::open_read_only, dst);
  const auto v = manager.find<int>("v");
  ASSERT_NE(v.first, nullptr);
  EXPECT_EQ(*v.first, 21);
}

// Copying onto a datastore that is open elsewhere is refused.
TEST(DurabilityTest, CopyRefusedOntoOpenDatastore) {
  const auto src = ds_path("copy_busy_src");
  const auto dst = ds_path("copy_busy_dst");
  metall::manager::remove(src);
  metall::manager::remove(dst);

  {
    metall::manager manager(metall::create_only, src);
    manager.construct<int>("v")(1);
  }

  metall::manager destination_manager(metall::create_only, dst);
  ASSERT_TRUE(destination_manager.check_sanity());

  EXPECT_FALSE(metall::manager::copy(src, dst));
  EXPECT_TRUE(destination_manager.check_sanity());
}

}  // namespace
