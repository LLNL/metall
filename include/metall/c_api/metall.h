// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_C_SUPORT_METALL_H
#define METALL_C_SUPORT_METALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define METALL_CREATE 1
#define METALL_OPEN 2
#define METALL_OPEN_OR_CREATE 3

extern int metall_open(int mode, const char *path, uint64_t size);
extern void metall_close();
extern void metall_flush();
extern void *metall_malloc(uint64_t size);
extern void metall_free(void *ptr);
extern void* metall_named_malloc(const char *name, uint64_t size);
extern void* metall_find(char *name);
extern void metall_named_free(const char *name);

#ifdef __cplusplus
}
#endif

#endif //METALL_C_SUPORT_METALL_H
