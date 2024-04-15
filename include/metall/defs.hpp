// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file defs.hpp
/// \brief Common definitions for Metall

#ifndef METALL_DEFS_HPP
#define METALL_DEFS_HPP

/// \def METALL_MAX_CAPACITY
/// The max capacity, i.e., the maximum total memory size a single Metall
/// datastore can allocate. This value is a theoretical limit, and the actual
/// limit is smaller than this value. This value is used to determine the types
/// of some internal variables.
#ifndef METALL_MAX_CAPACITY
#define METALL_MAX_CAPACITY (1ULL << 47ULL)
#endif

/// \def METALL_DEFAULT_CAPACITY
/// The default Metall store capacity, i.e., the default maximum total memory
/// size a single Metall datastore can allocate. This value is used when a new
/// Metall datastore is created without the capacity parameter. This value is a
/// theoretical limit, and the actual limit is smaller than this value. This
/// value must be less than or equal to METALL_MAX_CAPACITY.
#ifndef METALL_DEFAULT_CAPACITY
#if defined(__linux__)
#define METALL_DEFAULT_CAPACITY (1ULL << 43ULL)
#else
#define METALL_DEFAULT_CAPACITY (1ULL << 42ULL)
#endif
#endif

#ifdef DOXYGEN_SKIP
/// \brief If defined, Metall shows warning messages at compile time if the
/// system does not support important features.
#define METALL_VERBOSE_SYSTEM_SUPPORT_WARNING

/// \brief If defined, Metall stores addresses in sorted order in the bin
/// directory. This option enables Metall to use memory space more efficiently,
/// but it increases the cost of the bin directory operations.
#define METALL_USE_SORTED_BIN

/// \brief If defined, Metall tries to free space when an object equal to or
/// larger than the specified bytes is deallocated. Will be rounded up to a
/// multiple of the page size internally.
#define METALL_FREE_SMALL_OBJECT_SIZE_HINT
#endif

// --------------------
// Macros for the default segment storage
// --------------------

/// \def METALL_SEGMENT_BLOCK_SIZE
/// The segment block size the default segment storage use.
#ifndef METALL_SEGMENT_BLOCK_SIZE
#define METALL_SEGMENT_BLOCK_SIZE (1ULL << 28ULL)
#endif

#ifdef DOXYGEN_SKIP
/// \brief If defined, the default segment storage does not free file space even
/// thought the corresponding segment becomes free.
#define METALL_DISABLE_FREE_FILE_SPACE
#endif

// --------------------
// Macros for the object cache
// --------------------

/// \def METALL_MAX_PER_CPU_CACHE_SIZE
/// The maximum size of the per CPU (logical CPU core) cache in bytes.
#ifndef METALL_MAX_PER_CPU_CACHE_SIZE
#define METALL_MAX_PER_CPU_CACHE_SIZE (1ULL << 20ULL)
#endif

/// \def METALL_NUM_CACHES_PER_CPU
/// The number of caches per CPU (logical CPU core).
#ifndef METALL_NUM_CACHES_PER_CPU
#define METALL_NUM_CACHES_PER_CPU 2
#endif

#ifdef DOXYGEN_SKIP
/// \brief A macro to disable concurrency support.
/// \details
/// If this macro is defined, Metall disables concurrency support and optimizes
/// the internal behavior for single-thread usage. Applications must not call
/// any Metall functions concurrently if this macro is defined. On the other
/// hand, Metall still may use multi-threading for internal operations, such
/// as synchronizing data with files.
#define METALL_DISABLE_CONCURRENCY
#endif

// --------------------
// Deprecated macros

#ifdef METALL_MAX_SEGMENT_SIZE
#warning \
    "METALL_MAX_SEGMENT_SIZE is deprecated. Use METALL_MAX_CAPACITY instead."
#endif

#ifdef METALL_DEFAULT_VM_RESERVE_SIZE
#warning \
    "METALL_DEFAULT_VM_RESERVE_SIZE is deprecated. Use METALL_DEFAULT_CAPACITY instead."
#endif

#ifdef METALL_INITIAL_SEGMENT_SIZE
#warning \
    "METALL_INITIAL_SEGMENT_SIZE is deprecated. Use METALL_SEGMENT_BLOCK_SIZE instead."
#endif

#endif  // METALL_DEFS_HPP
