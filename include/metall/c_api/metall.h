// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_C_API_METALL_H
#define METALL_C_API_METALL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Opaque struct representing a metall manager
 * \note this type is internally represented by `::metall::manager` therefore pointers
 *    to `::metall_manager` may be reinterpret-casted to pointers to `::metall::manager`
 */
typedef struct metall_manager metall_manager;

/**
 * \brief Attempts to open the metall datastore at path
 * \param path path to datastore
 * \return true on success, false on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given metall datastore is inconsistent
 */
metall_manager* metall_open(const char* path);

/**
 * \brief Attempts to open the metall datastore at path in read only mode
 * \param path path to datastore
 * \return true on success, false on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given metall datastore is inconsistent
 */
metall_manager* metall_open_read_only(const char* path);

/**
 * \brief Attempts to create a metall datastore at path
 * \param path path at which to create a datastore
 * \return true on success, false on failure. On failure, sets errno to one of the following values:
 *      - EEXIST if the given path already exists
 *      - ENOTRECOVERABLE if the datastore could not be created for some other reason
 */
metall_manager* metall_create(const char* path);

/**
 * \brief Creates a snapshot of the metall datastore of manager and places it at dst_path
 * \param manager manager to perform snapshot
 * \param dst_path path where to place the snapshot
 * \return true if the snapshot was successfully created otherwise false.
 */
bool metall_snapshot(metall_manager* manager, const char* dst_path);

/**
 * \brief Flushes the given manager
 * \param manager manager to flush
 */
void metall_flush(metall_manager* manager);

/**
 * \brief Closes a metall manager
 */
void metall_close(metall_manager* manager);

/**
 * \brief Removes the metall datastore at path
 * \param path path to datastore to remove
 * \return true on successful removal, false otherwise. On failure, sets errno to one of the following values:
 *      - EADDRINUSE if there is a metall manager open for the given path
 *
 * \warning Behaviour is undefined if there is still a metall manager for path open
 */
bool metall_remove(const char* path);

/**
 * \brief Allocates size bytes
 * \param manager manager to allocate with
 * \param size number of bytes to allocate
 * \return pointer to allocated memory if successful otherwise returns NULL and sets errno to one of the following values
 *    - EINVAL
 */
void* metall_malloc(metall_manager* manager, size_t size);

/**
 * \brief Frees memory previously allocated by metall_malloc
 * \param manager manager from which to free
 * \param ptr memory to free
 */
void metall_free(metall_manager* manager, void* ptr);

/**
 * \brief Allocates size bytes and associates the allocated memory with a name
 * \param manager manager to allocate with
 * \param name A name of the allocated memory
 * \param size number of bytes to allocate
 * \return pointer to the allocated memory if sucessful otherwise returns NULL and sets errno to one of the following values
 *      - EINVAL if the given path does not have a metall datastore open
 *      - ENOMEM if the memory could not be allocated
 */
void* metall_named_malloc(metall_manager* manager, const char* name,
                          size_t size);

/**
 * \brief Finds memory that was previously allocated using metall_named_alloc
 * \param manager manager to find the object in
 * \param name name of the allocated memory to find
 * \return pointer to the allocated memory if found. Otherwise, returns NULL and sets errno to one of the following values
 *      - EINVAL if the given path does not have a metall datastore open
 *      - ENOTENT if the object could not be found
 */
void* metall_find(metall_manager* manager, const char* name);

/**
 * \brief Frees memory previously allocated by metall_named_malloc
 * \param manager manager from which to free
 * \param name name of the allocated memory to free
 * \return true if sucessfully freed, otherwise returns false and sets errno to one of the following values
 *      - EINVAL if the given path does not have a metall datastore open
 *      - ENOENT if the referred to object does not exist
 */
bool metall_named_free(metall_manager* manager, const char* name);

#ifdef __cplusplus
}
#endif

/// \example c_api.c
/// This is an example of how to use the C API.

#endif  // METALL_C_API_METALL_H