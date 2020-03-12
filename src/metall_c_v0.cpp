// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/c_api/metall.h>
#include <metall/metall.hpp>

metall::manager *g_manager = nullptr;

int metall_open(const int mode, const char *const path, const uint64_t size) {
  if (mode == METALL_CREATE) {
    g_manager = new metall::manager(metall::create_only, path, size);
  } else if (mode == METALL_OPEN) {
    g_manager = new metall::manager(metall::open_only, path);
  } else if (mode == METALL_OPEN_OR_CREATE) {
    g_manager = new metall::manager(metall::open_or_create, path, size);
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

void *metall_malloc(const uint64_t size) {
  return g_manager->allocate(size);
}

void metall_free(void *const ptr) {
  g_manager->deallocate(ptr);
}

void *metall_named_malloc(const char *name, const uint64_t size) {
  return g_manager->construct<char>(name)[size]();
}

void *metall_find(char *name) {
  return g_manager->find<char>(name).first;
}

void metall_named_free(const char *name) {
  g_manager->destroy<char>(name);
}
