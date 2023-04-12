// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_C_API_METALL_H
#define METALL_C_API_METALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Tag to create the segment always.
/// The existing segment with the same name is over written.
#define METALL_CREATE_ONLY 1

/// \brief Tag to open an already created segment.
#define METALL_OPEN_ONLY 2

/// \brief Tag to open an already created segment as read only.
#define METALL_OPEN_READ_ONLY 3

/// \brief Constructs a Metall manager object
/// \param mode Open mode
/// \param path A path to the backing data store
/// \return On success, returns 0. On error, returns -1.
extern int metall_open(int mode, const char *path);

/// \brief Destructs Metall manager object
extern void metall_close();

/// \brief Flush data to persistent memory
extern void metall_flush();

/// \brief Allocates nbytes bytes.
/// \param nbytes The Number of bytes to allocate
/// \return Returns a pointer to the allocated memory
extern void *metall_malloc(uint64_t nbytes);

/// \brief Frees the allocated memory
/// \param ptr A pointer to the allocated memory to be free
extern void metall_free(void *ptr);

/// \brief Allocates nbytes bytes and save the address of the allocated memory
/// with name \param name A name of the allocated memory \param nbytes A size to
/// allocate \return Returns a pointer to the allocated memory
extern void *metall_named_malloc(const char *name, uint64_t nbytes);

/// \brief Finds a saved memory
/// \param name A name of the allocated memory to find
/// \return Returns a pointer to the allocated memory if it exist. Otherwise,
/// returns NULL.
extern void *metall_find(char *name);

/// \brief Frees memory with the name
/// \param name A name of the allocated memory to free
extern void metall_named_free(const char *name);

/// \brief Snapshot the entire data.
/// \param destination_path The path to store a snapshot.
/// \return On success, returns 0. On error, returns -1.
extern int snapshot(const char *destination_path);

/// \brief Copies backing files synchronously.
/// \param source_path Source data store path.
/// \param destination_path Destination data store path.
/// \return On success, returns 0. On error, returns -1.
extern int copy(const char *source_path, const char *destination_path);

/// \brief Check if the backing data store is consistent,
/// i.e. it was closed properly.
/// \param path A path to the backing data store.
/// \return Returns a oon-zero integer if the data store is consistent;
/// otherwise, returns 0.
extern int consistent(const char *path);

#ifdef __cplusplus
}
#endif

/// \example c_api.c
/// This is an example of how to use the C API.

#endif  // METALL_C_API_METALL_H