// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/c_api/metall.h>
#include <metall/metall.hpp>

template <typename open_mode>
metall_manager* open_impl(const char* path) {
  if (!metall::manager::consistent(path)) {
    // prevents opening the same datastore twice
    // (because opening removes the properly_closed_mark and this checks for it)
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  auto* manager = new metall::manager{open_mode{}, path};
  if (!manager->check_sanity()) {
    delete manager;
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  return reinterpret_cast<metall_manager*>(manager);
}

metall_manager* metall_open(const char* path) {
  return open_impl<metall::open_only_t>(path);
}

metall_manager* metall_open_read_only(const char* path) {
  return open_impl<metall::open_read_only_t>(path);
}

metall_manager* metall_create(const char* path) {
  if (std::filesystem::exists(path)) {
    // prevent accidental overwrite
    errno = EEXIST;
    return nullptr;
  }

  auto* manager = new metall::manager{metall::create_only, path};
  if (!manager->check_sanity()) {
    delete manager;
    errno = ENOTRECOVERABLE;
    return nullptr;
  }

  return reinterpret_cast<metall_manager*>(manager);
}

bool metall_snapshot(metall_manager* manager, const char* dst_path) {
  return reinterpret_cast<metall::manager*>(manager)->snapshot(dst_path);
}

void metall_flush(metall_manager* manager) {
  reinterpret_cast<metall::manager*>(manager)->flush();
}

void metall_close(metall_manager* manager) {
  delete reinterpret_cast<metall::manager*>(manager);
}

bool metall_remove(const char* path) {
  return metall::manager::remove(path);
}

void* metall_malloc(metall_manager* manager, size_t size) {
  auto* ptr = reinterpret_cast<metall::manager*>(manager)->allocate(size);
  if (ptr == nullptr) {
    errno = ENOMEM;
  }

  return ptr;
}

void metall_free(metall_manager* manager, void* ptr) {
  reinterpret_cast<metall::manager*>(manager)->deallocate(ptr);
}

void* metall_named_malloc(metall_manager* manager, const char* name,
                          size_t size) {
  auto* ptr = reinterpret_cast<metall::manager*>(manager)->construct<unsigned
    char>(name)[size]();
  if (ptr == nullptr) {
    errno = ENOMEM;
  }

  return ptr;
}

void* metall_find(metall_manager* manager, const char* name) {
  auto* ptr = reinterpret_cast<metall::manager*>(manager)->find<unsigned
    char>(name).first;
  if (ptr == nullptr) {
    errno = ENOENT;
  }

  return ptr;
}

bool metall_named_free(metall_manager* manager, const char* name) {
  auto const res = reinterpret_cast<metall::manager*>(manager)->destroy<unsigned
    char>(name);
  if (!res) {
    errno = ENOENT;
  }

  return res;
}