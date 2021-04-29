// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BASIC_MANAGER_HPP
#define METALL_BASIC_MANAGER_HPP

#include <cstddef>
#include <memory>

#include <metall/tags.hpp>
#include <metall/stl_allocator.hpp>
#include <metall/container/scoped_allocator.hpp>
#include <metall/kernel/manager_kernel.hpp>
#include <metall/detail/named_proxy.hpp>

namespace metall {

#if !defined(DOXYGEN_SKIP)
// Forward declaration
template <typename chunk_no_type, std::size_t k_chunk_size>
class basic_manager;
#endif // DOXYGEN_SKIP

/// \brief A generalized Metall manager class
/// \tparam chunk_no_type
/// \tparam k_chunk_size
template <typename chunk_no_type = uint32_t, std::size_t k_chunk_size = (1ULL << 21ULL)>
class basic_manager {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //

  /// \brief Manager kernel type
  using manager_kernel_type = kernel::manager_kernel<chunk_no_type, k_chunk_size>;

  /// \brief Void pointer type
  using void_pointer = typename manager_kernel_type::void_pointer;

  /// \brief Char type
  using char_type = typename manager_kernel_type::char_type;

  /// \brief Size type
  using size_type = typename manager_kernel_type::size_type;

  /// \brief Difference type
  using difference_type = typename manager_kernel_type::difference_type;

  /// \brief Allocator type
  template <typename T>
  using allocator_type = stl_allocator<T, manager_kernel_type>;

  /// \brief Allocator type wrapped by scoped_allocator_adaptor
  template <typename OuterT, typename... InnerT>
  using scoped_allocator_type = container::scoped_allocator_adaptor<allocator_type<OuterT>, allocator_type<InnerT>...>;

  /// \brief Construct proxy
  template <typename T>
  using construct_proxy = metall::mtlldetail::named_proxy<manager_kernel_type, T, false>;

  /// \brief Construct iterator proxy
  template <typename T>
  using construct_iter_proxy = metall::mtlldetail::named_proxy<manager_kernel_type, T, true>;

  /// \brief An value that describes the type of the instance constructed in memory
  using instance_kind = typename manager_kernel_type::instance_kind;

  /// \brief Const iterator for named objects
  using const_named_iterator = typename manager_kernel_type::const_named_iterator;

  /// \brief Const iterator for unique objects
  using const_unique_iterator = typename manager_kernel_type::const_unique_iterator;

  /// \brief Const iterator for anonymous objects
  using const_anonymous_iterator = typename manager_kernel_type::const_anonymous_iterator;

  /// \brief Provides access to named object attribute
  using named_object_attribute_accessor_type = typename manager_kernel_type::named_object_attr_accessor_type;

  /// \brief Provides access to unique object attribute
  using unique_object_attribute_accessor_type = typename manager_kernel_type::unique_object_attr_accessor_type;

  /// \brief Provides access to anonymous object attribute
  using anonymous_object_attribute_accessor_type = typename manager_kernel_type::anonymous_object_attr_accessor_type;

  /// \brief Chunk number type (= chunk_no_type)
  using chunk_number_type = chunk_no_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using char_ptr_holder_type = typename manager_kernel_type::char_ptr_holder_type;
  using self_type = basic_manager<chunk_no_type, k_chunk_size>;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //

  /// \brief Opens an existing data store.
  /// \param base_path Path to a data store.
  basic_manager(open_only_t, const char *base_path) noexcept {
    try {
      m_kernel = std::make_unique<manager_kernel_type>();
      m_kernel->open(base_path);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  /// \brief Opens an existing data store with the read only mode.
  /// Write accesses will cause segmentation fault.
  /// \param base_path Path to a data store.
  basic_manager(open_read_only_t, const char *base_path) noexcept {
    try {
      m_kernel = std::make_unique<manager_kernel_type>();
      m_kernel->open_read_only(base_path);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  /// \brief Creates a new data store (an existing data store will be overwritten).
  /// \param base_path Path to create a data store.
  basic_manager(create_only_t, const char *base_path) noexcept {
    try {
      m_kernel = std::make_unique<manager_kernel_type>();
      m_kernel->create(base_path);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  /// \brief Creates a new data store (an existing data store will be overwritten).
  /// \param base_path Path to create a data store.
  /// \param capacity Maximum total allocation size.
  basic_manager(create_only_t, const char *base_path, const size_type capacity) noexcept {
    try {
      m_kernel = std::make_unique<manager_kernel_type>();
      m_kernel->create(base_path, capacity);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  /// \brief Deleted.
  basic_manager() = delete;

  /// \brief Destructor.
  ~basic_manager() noexcept = default;

  /// \brief Deleted.
  basic_manager(const basic_manager &) = delete;

  /// \brief Move constructor.
  basic_manager(basic_manager &&) noexcept = default;

  /// \brief Deleted.
  /// \return N/A.
  basic_manager &operator=(const basic_manager &) = delete;

  /// \brief Move assignment operator.
  /// \return An reference to the object.
  basic_manager &operator=(basic_manager &&) noexcept = default;

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //

  // -------------------- Attributed object construction function family -------------------- //
  // Each function also works with '[ ]' operator to generate an array, leveraging the proxy class (construct_proxy)

  /// \private
  /// \class common_doc_const_find
  /// \brief
  /// Object construction API developed by Boost.Interprocess
  /// <a href="https://www.boost.org/doc/libs/release/doc/html/interprocess/managed_memory_segments.html#interprocess.managed_memory_segments.managed_memory_segment_features.allocation_types">
  /// (see details).
  /// </a>
  ///
  /// A named object must be associated with non-empty name.
  /// The name of an unique object is typeid(T).name().

  /// \brief Allocates an object of type T.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// If T's constructor throws, the function throws that exception.
  /// Memory is freed automatically if T's constructor throws and if an
  /// array was being constructed, destructors of created objects are called
  /// before freeing the memory.
  ///
  /// Example:
  /// \code
  /// T *ptr = basic_manager.construct<T>("Name")(arg1, arg2...);
  /// T *ptr = basic_manager.construct<T>("Name")[count](arg1, arg2...);
  /// \endcode
  /// Where, 'arg1, arg2...' are the arguments passed to T's constructor via a proxy object.
  /// One can also construct an array using '[ ]' operator.
  /// When an array is constructed, each object receives the same arguments.
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that constructs the object on the allocated space.
  /// Returns nullptr if the name was used or it failed to allocate memory.
  template <typename T>
  construct_proxy<T> construct(char_ptr_holder_type name) {
    return construct_proxy<T>(m_kernel.get(), name, false, false);
  }

  /// \brief Tries to find an already constructed object. If not exist, constructs an object of type T.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// If T's constructor throws, the function throws that exception.
  /// Memory is freed automatically if T's constructor throws and if an
  /// array was being constructed, destructors of created objects are called
  /// before freeing the memory.
  ///
  /// Example:
  /// \code
  /// T *ptr = basic_manager.find_or_construct<T>("Name")(arg1, arg2...);
  /// T *ptr = basic_manager.find_or_construct<T>("Name")[count](arg1, arg2...);
  /// \endcode
  /// Where, 'arg1, arg2...' are the arguments passed to T's constructor via a proxy object.
  /// One can also construct an array using '[ ]' operator.
  /// When an array is constructed, each object receives the same arguments.
  ///
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return A proxy object that holds a pointer of an already constructed object or an object newly constructed.
  template <typename T>
  construct_proxy<T> find_or_construct(char_ptr_holder_type name) {
    return construct_proxy<T>(m_kernel.get(), name, true, false);
  }

  /// \brief Allocates an array of objects of type T, receiving arguments from iterators.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// If T's constructor throws, the function throws that exception.
  /// Memory is freed automatically if T's constructor throws and if an
  /// array was being constructed, destructors of created objects are called
  /// before freeing the memory.
  ///
  /// Example:
  /// \code
  /// T *ptr = basic_manager.construct_it<T>("Name")[count](it1, it2...);
  /// \endcode
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object to construct an array of objects.
  template <typename T>
  construct_iter_proxy<T> construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(m_kernel.get(), name, false, false);
  }

  /// \brief Tries to find an already constructed object.
  /// If not exist, constructs an array of objects of type T, receiving arguments from iterators.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// If T's constructor throws, the function throws that exception.
  /// Memory is freed automatically if T's constructor throws and if an
  /// array was being constructed, destructors of created objects are called
  /// before freeing the memory.
  ///
  /// Example:
  /// \code
  /// T *ptr = basic_manager.find_or_construct_it<T>("Name")[count](it1, it2...);
  /// \endcode
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that holds a pointer to the already constructed object or
  /// constructs an array of objects or just holds an pointer.
  template <typename T>
  construct_iter_proxy<T> find_or_construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(m_kernel.get(), name, true, false);
  }

  /// \brief Tries to find a previously created object.
  /// \copydoc common_doc_const_find
  /// \warning There is no mutex inside.
  /// Calling this function with other construct/destroy methods that updates an object directory simultaneously
  /// will cause a concurrent issue.
  ///
  /// \details
  /// Example:
  /// \code
  /// std::pair<T *, std::size_t> ret = basic_manager.find<T>("Name");
  /// \endcode
  ///
  /// \tparam T  The type of the object.
  /// \param name The name of the object.
  /// \return Returns a pointer to the object and the count (if it is not an array, returns 1).
  /// If not present, nullptr is returned.
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) const noexcept {
    if (!check_sanity()) {
      return std::make_pair(nullptr, 0);
    }

    try {
      return m_kernel->template find<T>(name);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }

    return std::make_pair(nullptr, 0);
  }

  /// \brief Destroys a previously created object.
  /// Calls the destructor and frees the memory.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  ///
  /// \note
  /// If T's destructor throws:
  /// 1) the exception will be thrown (propagated);
  /// 2) the memory will won't be freed;
  /// 3) the object entry will be still removed from the attributed object directory.
  /// Therefore, it is not recommended to throw exception in a destructor.
  ///
  /// Example:
  /// \code
  /// bool destroyed = basic_manager.destroy<T>("Name");
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return Returns false if the object was not destroyed.
  template <typename T>
  bool destroy(const char *name) {
    if (!check_sanity()) {
      return false;
    }
    return m_kernel->template destroy<T>(name);
  }

  /// \brief Destroys a unique object of type T.
  /// Calls the destructor and frees the memory.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  ///
  /// \note
  /// If T's destructor throws:
  /// 1) the exception will be thrown (propagated);
  /// 2) the memory will won't be freed;
  /// 3) the object entry will be still removed from the attributed object directory.
  /// Therefore, it is not recommended to throw exception in a destructor.
  ///
  /// Example:
  /// \code
  /// bool destroyed = basic_manager.destroy<T>(metall::unique_instance);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \return Returns false if the object was not destroyed.
  template <typename T>
  bool destroy(const metall::mtlldetail::unique_instance_t *const) {
    if (!check_sanity()) {
      return false;
    }
    return m_kernel->template destroy<T>(metall::unique_instance);
  }

  /// \brief Destroys a object (named, unique, or anonymous) by its address.
  /// Calls the destructor and frees the memory.
  /// Cannot destroy an object not allocated by construct/find_or_construct functions.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  ///
  /// \note
  /// If T's destructor throws:
  /// 1) the exception will be thrown (propagated);
  /// 2) the memory will won't be freed;
  /// 3) the object entry will be still removed from the attributed object directory.
  /// Therefore, it is not recommended to throw exception in a destructor.
  ///
  /// Example:
  /// \code
  /// bool destroyed = basic_manager.destroy_ptr<T>(ptr);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \return Returns false if the object was not destroyed.
  /// Note that the original API developed by Boost.Interprocess library does not return value.
  template <class T>
  bool destroy_ptr(const T *ptr) {
    if (!check_sanity()) {
      return false;
    }
    return m_kernel->template destroy_ptr<T>(ptr);
  }

  /// \brief Returns the name of an object created with construct/find_or_construct functions.
  ///
  /// \details
  /// Example:
  /// \code
  /// const char_type name = basic_manager.get_instance_name<T>(ptr);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \return The name of the object.
  /// If ptr points to an unique instance, typeid(T).name() is returned.
  /// If ptr points to an anonymous instance or memory not allocated by construct/find_or_construct functions,
  /// nullptr is returned.
  template <class T>
  const char_type *get_instance_name(const T *ptr) const noexcept {
    if (!check_sanity()) {
      return nullptr;
    }
    try {
      return m_kernel->get_instance_name(ptr);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return nullptr;
  }

  /// \brief Returns the kind of an object created with construct/find_or_construct functions.
  ///
  /// \details
  /// Example:
  /// \code
  /// const instance_kind t = basic_manager.get_instance_kind<T>(ptr);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \return The type of the object.
  template <class T>
  instance_kind get_instance_kind(const T *ptr) const noexcept {
    if (!check_sanity()) {
      return instance_kind();
    }
    try {
      return m_kernel->get_instance_kind(ptr);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return instance_kind();
  }

  /// \brief Returns the length of an object created with construct/find_or_construct
  /// functions (1 if is a single element, >=1 if it's an array).
  ///
  /// \details
  /// Example:
  /// \code
  /// const size_type length = basic_manager.get_instance_length<T>(ptr);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \return The type of the object.
  template <class T>
  size_type get_instance_length(const T *ptr) const noexcept {
    if (!check_sanity()) {
      return 0;
    }
    try {
      return m_kernel->get_instance_length(ptr);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return 0;
  }

  /// \brief Checks if the type of an object, which was created with construct/find_or_construct
  /// functions, is T.
  ///
  /// \details
  /// Example:
  /// \code
  /// const bool correct_type = basic_manager.type<T>(ptr);
  /// \endcode
  ///
  /// \tparam T A expected type of the object.
  /// \param ptr A pointer to the object.
  /// \return Returns true if T is correct; otherwise false.
  template <class T>
  bool is_instance_type(const void *const ptr) const noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->template is_instance_type<T>(ptr);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Gets the description of an object created with construct/find_or_construct
  ///
  /// \details
  /// Example:
  /// \code
  /// std::string description;
  /// basic_manager.get_instance_description<T>(ptr, &description);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \param description A pointer to a string buffer.
  /// \return Returns false on error.
  template <class T>
  bool get_instance_description(const T *ptr, std::string *description) const noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->get_instance_description(ptr, description);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Sets a description to an object created with construct/find_or_construct
  ///
  /// \details
  /// Example:
  /// \code
  /// std::string description;
  /// basic_manager.set_instance_description<T>(ptr, description);
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param ptr A pointer to the object.
  /// \param description A description to set.
  /// \return Returns false on error.
  template <class T>
  bool set_instance_description(const T *ptr, const std::string &description) noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->set_instance_description(ptr, description);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Returns Returns the number of named objects stored in the managed segment.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return The number of named objects stored in the managed segment.
  size_type get_num_named_objects() const noexcept {
    if (!check_sanity()) {
      return 0;
    }
    try {
      return m_kernel->get_num_named_objects();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return 0;
  }

  /// \brief Returns Returns the number of unique objects stored in the managed segment.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return The number of unique objects stored in the managed segment.
  size_type get_num_unique_objects() const noexcept {
    if (!check_sanity()) {
      return 0;
    }
    try {
      return m_kernel->get_num_unique_objects();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return 0;
  }

  /// \brief Returns Returns the number of anonymous objects (objects constructed with metall::anonymous_instance)
  /// stored in the managed segment.
  /// \return The number of anonymous objects stored in the managed segment.
  size_type get_num_anonymous_objects() const noexcept {
    if (!check_sanity()) {
      return 0;
    }
    try {
      return m_kernel->get_num_anonymous_objects();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return 0;
  }

  /// \brief Returns a constant iterator to the index storing the named objects.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return A constant iterator to the index storing the named objects.
  const_named_iterator named_begin() const noexcept {
    if (!check_sanity()) {
      return const_named_iterator();
    }
    try {
      return m_kernel->named_begin();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_named_iterator();
  }

  /// \brief Returns a constant iterator to the end of the index storing the named allocations.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return A constant iterator.
  const_named_iterator named_end() const noexcept {
    if (!check_sanity()) {
      return const_named_iterator();
    }
    try {
      return m_kernel->named_end();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_named_iterator();
  }

  /// \brief Returns a constant iterator to the index storing the unique objects.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return A constant iterator to the index storing the unique objects.
  const_unique_iterator unique_begin() const noexcept {
    if (!check_sanity()) {
      return const_unique_iterator();
    }
    try {
      return m_kernel->unique_begin();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_unique_iterator();
  }

  /// \brief Returns a constant iterator to the end of the index
  /// storing the unique allocations. NOT thread-safe. Never throws.
  ///
  /// \details
  /// \copydoc common_doc_const_find
  ///
  /// \return A constant iterator.
  const_unique_iterator unique_end() const noexcept {
    if (!check_sanity()) {
      return const_unique_iterator();
    }
    try {
      return m_kernel->unique_end();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_unique_iterator();
  }

  /// \brief Returns a constant iterator to the index storing the anonymous objects.
  /// \return A constant iterator to the index storing the anonymous objects.
  const_anonymous_iterator anonymous_begin() const noexcept {
    if (!check_sanity()) {
      return const_anonymous_iterator();
    }
    try {
      return m_kernel->anonymous_begin();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_anonymous_iterator();
  }

  /// \brief Returns a constant iterator to the end of the index
  /// storing the anonymous allocations. NOT thread-safe. Never throws.
  /// \return A constant iterator.
  const_anonymous_iterator anonymous_end() const noexcept {
    if (!check_sanity()) {
      return const_anonymous_iterator();
    }
    try {
      return m_kernel->anonymous_end();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return const_anonymous_iterator();
  }

  // TODO: implement
  // bool belongs_to_segment (const void *ptr) const;

  // -------------------- Allocate memory by size -------------------- //
  /// \brief Allocates nbytes bytes.
  /// \param nbytes Number of bytes to allocate.
  /// \return Returns a pointer to the allocated memory.
  void *allocate(size_type nbytes) noexcept {
    if (!check_sanity()) {
      return nullptr;
    }
    try {
      return m_kernel->allocate(nbytes);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return nullptr;
  }

  /// \brief Allocates nbytes bytes. The address of the allocated memory will be a multiple of alignment.
  /// \param nbytes Number of bytes to allocate. Must be a multiple alignment.
  /// \param alignment Alignment size.
  /// Alignment must be a power of two and satisfy [min allocation size, chunk size].
  /// \return Returns a pointer to the allocated memory.
  void *allocate_aligned(size_type nbytes, size_type alignment) noexcept {
    if (!check_sanity()) {
      return nullptr;
    }
    try {
      return m_kernel->allocate_aligned(nbytes, alignment);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return nullptr;
  }

  // void allocate_many(const std::nothrow_t &tag, size_type elem_bytes, size_type n_elements, multiallocation_chain &chain);

  /// \brief Deallocates the allocated memory.
  /// \param addr A pointer to the allocated memory to be deallocated.
  void deallocate(void *addr) noexcept {
    if (!check_sanity()) {
      return;
    }
    try {
      return m_kernel->deallocate(addr);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  // void deallocate_many(multiallocation_chain &chain);

  // Returns true if all allocated memory has been deallocated
  // bool all_memory_deallocated();

  // -------------------- Flush -------------------- //
  /// \brief Flush data to persistent memory.
  /// \param synchronous If true, performs synchronous operation;
  /// otherwise, performs asynchronous operation.
  void flush(const bool synchronous = true) noexcept {
    if (!check_sanity()) {
      return;
    }
    try {
      m_kernel->flush(synchronous);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }

  // -------------------- Snapshot, copy, data store management -------------------- //
  /// \brief Takes a snapshot of the current data. The snapshot has a new UUID.
  /// \param destination_dir_path Path to store a snapshot.
  /// \param clone Use the file clone mechanism (reflink) instead of normal copy if it is available.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Returns true on success; other false.
  bool snapshot(const char_type *destination_dir_path,
                const bool clone = true,
                const int num_max_copy_threads = 0) noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->snapshot(destination_dir_path, clone, num_max_copy_threads);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Copies data store synchronously.
  /// The behavior of copying a data store that is open without the read-only mode is undefined.
  /// \param source_dir_path Source data store path.
  /// \param destination_dir_path Destination data store path.
  /// \param clone Use the file clone mechanism (reflink) instead of normal copy if it is available.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return If succeeded, returns true; other false.
  static bool copy(const char_type *source_dir_path,
                   const char_type *destination_dir_path,
                   const bool clone = true,
                   const int num_max_copy_threads = 0) noexcept {
    try {
      return manager_kernel_type::copy(source_dir_path, destination_dir_path, clone, num_max_copy_threads);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Copies data store asynchronously.
  /// The behavior of copying a data store that is open without the read-only mode is undefined.
  /// \param source_dir_path Source data store path.
  /// \param destination_dir_path Destination data store path.
  /// \param clone Use the file clone mechanism (reflink) instead of normal copy if it is available.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Returns an object of std::future.
  /// If succeeded, its get() returns true; other false.
  static auto copy_async(const char_type *source_dir_path,
                         const char_type *destination_dir_path,
                         const bool clone = true,
                         const int num_max_copy_threads = 0) noexcept {
    try {
      return manager_kernel_type::copy_async(source_dir_path, destination_dir_path, clone, num_max_copy_threads);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return std::future<bool>();
  }

  /// \brief Removes data store synchronously.
  /// \param dir_path Path to a data store to remove.
  /// \return If succeeded, returns true; other false.
  static bool remove(const char_type *dir_path) noexcept {
    try {
      return manager_kernel_type::remove(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Remove data store asynchronously.
  /// \param dir_path Path to a data store to remove.
  /// \return Returns an object of std::future.
  /// If succeeded, its get() returns true; other false
  static std::future<bool> remove_async(const char_type *dir_path) noexcept {
    try {
      return std::async(std::launch::async, remove, dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return std::future<bool>();
  }

  /// \brief Check if a data store exists and is consistent (i.e., it was closed properly in the previous run).
  /// \param dir_path Path to a data store.
  /// \return Returns true if it exists and is consistent; otherwise, returns false.
  static bool consistent(const char_type *dir_path) noexcept {
    try {
      return manager_kernel_type::consistent(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Returns a UUID of the data store.
  /// \return UUID in the std::string format; returns an empty string on error.
  std::string get_uuid() const noexcept {
    if (!check_sanity()) {
      return std::string();
    }
    try {
      return m_kernel->get_uuid();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return std::string();
  }

  /// \brief Returns a UUID of the data store.
  /// \param dir_path Path to a data store.
  /// \return UUID in the std::string format; returns an empty string on error.
  static std::string get_uuid(const char_type *dir_path) noexcept {
    try {
      return manager_kernel_type::get_uuid(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return std::string();
  }

  /// \brief Gets the version of the Metall that created the backing data store.
  /// \return Returns a version number; returns 0 on error.
  version_type get_version() const noexcept {
    if (!check_sanity()) {
      return version_type();
    }
    try {
      return m_kernel->get_version();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return version_type();
  }

  /// \brief Gets the version of the Metall that created the backing data store.
  /// \param dir_path Path to a data store.
  /// \return Returns a version number; returns 0 on error.
  static version_type get_version(const char_type *dir_path) noexcept {
    try {
      return manager_kernel_type::get_version(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
      return version_type();
    }
  }

  // -------------------- Data store description -------------------- //

  /// \brief Sets a description to a Metall data store.
  /// An existing description is overwritten (only one description per data store).
  /// \warning This method is not thread-safe.
  /// \param description An std::string object that holds a description.
  /// \return Returns true on success; otherwise, false.
  bool set_description(const std::string &description) noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->set_description(description);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Sets a description to a Metall data store.
  /// An existing description is overwritten (only one description per data store).
  /// \warning This function is not thread-safe.
  /// Updating the same data store with multiple threads simultaneously could cause an issue.
  /// \param dir_path Path to a data store.
  /// \param description An std::string object that holds a description.
  /// \return Returns true on success; otherwise, false.
  static bool set_description(const char *dir_path, const std::string &description) noexcept {
    try {
      return manager_kernel_type::set_description(dir_path, description);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Gets a description.
  /// If there is no description, nothing to happen to the given description object.
  /// \param description A pointer to an std::string object to store a description if it exists.
  /// \return Returns true on success; returns false on error.
  /// Trying to get a non-existent description is not considered as an error.
  bool get_description(std::string *description) const noexcept {
    if (!check_sanity()) {
      return false;
    }
    try {
      return m_kernel->get_description(description);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  /// \brief Gets a description.
  /// If there is no description, nothing to happen to the given description object.
  /// \param dir_path Path to a data store.
  /// \param description A pointer to an std::string object to store a description if it exists.
  /// \return Returns true on success; returns false on error.
  /// Trying to get a non-existent description is not considered as an error.
  static bool get_description(const char *dir_path, std::string *description) noexcept {
    try {
      return manager_kernel_type::get_description(dir_path, description);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return false;
  }

  // -------------------- Object attribute -------------------- //
  /// \brief Returns an instance that provides access to the attribute of named objects.
  /// \param dir_path Path to a data store.
  /// \return Returns an instance of named_object_attribute_accessor_type.
  static named_object_attribute_accessor_type access_named_object_attribute(const char *dir_path) noexcept {
    try {
      return manager_kernel_type::access_named_object_attribute(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return named_object_attribute_accessor_type();
  }

  /// \brief Returns an instance that provides access to the attribute of unique object.
  /// \param dir_path Path to a data store.
  /// \return Returns an instance of unique_object_attribute_accessor_type.
  static unique_object_attribute_accessor_type access_unique_object_attribute(const char *dir_path) noexcept {
    try {
      return manager_kernel_type::access_unique_object_attribute(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return unique_object_attribute_accessor_type();
  }

  /// \brief Returns an instance that provides access to the attribute of anonymous object.
  /// \param dir_path Path to a data store.
  /// \return Returns an instance of anonymous_object_attribute_accessor_type.
  static anonymous_object_attribute_accessor_type access_anonymous_object_attribute(const char *dir_path) noexcept {
    try {
      return manager_kernel_type::access_anonymous_object_attribute(dir_path);
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return anonymous_object_attribute_accessor_type();
  }

  // -------------------- etc -------------------- //
  /// \brief Returns a STL compatible allocator object.
  /// \tparam T Type of the object.
  /// \return Returns a STL compatible allocator object.
  template <typename T = std::byte>
  allocator_type<T> get_allocator() const noexcept {
    if (!check_sanity()) {
      return allocator_type<T>(nullptr);
    }
    try {
      return allocator_type<T>(reinterpret_cast<manager_kernel_type *const *>(&(m_kernel->get_segment_header()->manager_kernel_address)));
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return allocator_type<T>(nullptr);
  }

  /// \brief Returns the internal chunk size.
  /// \return The size of internal chunk size.
  static constexpr size_type chunk_size() noexcept {
    return k_chunk_size;
  }

  /// \brief Returns the address of the application data segment.
  /// \return The address of the application data segment.
  const void *get_address() const noexcept {
    if (!check_sanity()) {
      return nullptr;
    }
    try {
      return m_kernel->get_segment();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return nullptr;
  }

  /// \brief Returns the size (i.e., the maximum total allocation size) of the application data segment.
  /// This is a theoretical value. The actual total allocation size Metall can handle will be less than that.
  /// \return The size of the application data segment.
  size_type get_size() const noexcept {
    if (!check_sanity()) {
      return 0;
    }
    try {
      return m_kernel->get_segment_size();
    } catch (...) {
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
    return 0;
  }

  // bool belongs_to_segment (const void *ptr) const

  // Makes an internal sanity check
  // and returns true if success
  bool check_sanity() const noexcept {
    return !!m_kernel; // TODO: also implement m_kernel->check_sanity();
  }

  // -------------------- For profiling and debug -------------------- //
#if !defined(DOXYGEN_SKIP)
  /// \brief Prints out profiling information.
  /// \tparam out_stream_type A type of out stream.
  /// \param log_out An object of the out stream.
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) noexcept {
    if (!check_sanity()) {
      return;
    }
    try {
      m_kernel->profile(log_out);
    } catch (...) {
      m_kernel.reset(nullptr);
      logger::out(logger::level::error, __FILE__, __LINE__, "An exception has been thrown");
    }
  }
#endif

 private:
  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  std::unique_ptr<manager_kernel_type> m_kernel{nullptr};
};
} // namespace metall

#endif //METALL_BASIC_MANAGER_HPP
