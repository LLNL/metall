// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/c_api/metall.h>
#include <metall/metall.hpp>

metall::manager *g_manager = nullptr;

int metall_open(const int mode, const char *const path) {
  if (mode == METALL_CREATE_ONLY) {
    g_manager = new metall::manager(metall::create_only, path);
  } else if (mode == METALL_OPEN_ONLY) {
    g_manager = new metall::manager(metall::open_only, path);
  } else if (mode == METALL_OPEN_READ_ONLY) {
    g_manager = new metall::manager(metall::open_read_only, path);
  } else if (mode == METALL_OPEN_OR_CREATE) {
      g_manager = new metall::manager(metall::open_or_create, path);
  } else {
    g_manager = nullptr;
  }

  if (g_manager) {
    return 0;
  } else {
    return -1; // error
  }
}

void metall_close() {
  delete g_manager;
}

void metall_flush() {
  g_manager->flush();
}

void *metall_malloc(const uint64_t nbytes) {
  return g_manager->allocate(nbytes);
}

void metall_free(void *const ptr) {
  g_manager->deallocate(ptr);
}

void *metall_named_malloc(const char *name, const uint64_t nbytes) {
  return g_manager->construct<char>(name)[nbytes]();
}

void *metall_find(char *name) {
  return g_manager->find<char>(name).first;
}

void metall_named_free(const char *name) {
  g_manager->destroy<char>(name);
}

int snapshot(const char *destination_path) {
  if (g_manager->snapshot(destination_path)) return 0;
  return -1; // Error
}

int copy(const char *source_path, const char *destination_path) {
  if (metall::manager::copy(source_path, destination_path)) return 0;
  return -1; // Error
}

int consistent(const char *path) {
  if (metall::manager::consistent(path)) return 1;
  return 0;
}