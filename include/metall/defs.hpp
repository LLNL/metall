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
/// limit is smaller than this value.
#ifndef METALL_MAX_CAPACITY
#define METALL_MAX_CAPACITY (1ULL << 48ULL)
#endif

#ifdef METALL_MAX_SEGMENT_SIZE
#warning \
    "METALL_MAX_SEGMENT_SIZE is deprecated. Use METALL_MAX_CAPACITY instead."
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

#ifdef METALL_DEFAULT_VM_RESERVE_SIZE
#warning \
    "METALL_DEFAULT_VM_RESERVE_SIZE is deprecated. Use METALL_DEFAULT_CAPACITY instead."
#endif

/// \def METALL_SEGMENT_BLOCK_SIZE
/// The segment block size the default segment storage use.
#ifndef METALL_SEGMENT_BLOCK_SIZE
#define METALL_SEGMENT_BLOCK_SIZE (1ULL << 28ULL)
#endif

#ifdef METALL_INITIAL_SEGMENT_SIZE
#warning \
    "METALL_INITIAL_SEGMENT_SIZE is deprecated. Use METALL_SEGMENT_BLOCK_SIZE instead."
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

/// \def METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
/// Cache size per bin in bytes.
#ifndef METALL_CACHE_BIN_SIZE
#define METALL_CACHE_BIN_SIZE (1ULL << 20ULL)
#endif

/// \def METALL_NUM_CACHES_PER_CPU
/// The number of caches per CPU (logical CPU core).
#ifndef METALL_NUM_CACHES_PER_CPU
#define METALL_NUM_CACHES_PER_CPU 4
#endif

#endif  // METALL_DEFS_HPP
