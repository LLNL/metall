// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_MANAGER_V0_HPP
#define METALL_MANAGER_V0_HPP

#include <memory>

#include <metall/stl_allocator.hpp>
#include <metall/detail/base_manager.hpp>
#include <metall/v0/kernel/manager_kernel.hpp>
#include <metall/detail/utility/in_place_interface.hpp>
#include <metall/detail/utility/named_proxy.hpp>

namespace metall {
namespace v0 {

namespace {
namespace util = metall::detail::utility;
}

// Forward declaration
template <typename chunk_no_type, std::size_t k_chunk_size, typename kernel_allocator_type>
class manager_v0;

// Detailed types
namespace detail {
/// \brief This is just a utility class to hold important types in manager
/// \tparam chunk_no_type
/// \tparam k_chunk_size
/// \tparam kernel_allocator_type
template <typename chunk_no_type, std::size_t k_chunk_size, typename kernel_allocator_type>
struct manager_type_holder {
  using kernel_type = kernel::manager_kernel<chunk_no_type, k_chunk_size, kernel_allocator_type>;
  using void_pointer = typename kernel_type::void_pointer;
  using size_type = typename kernel_type::size_type;
  using difference_type = typename kernel_type::difference_type;
  template <typename T>
  using allocator_type = stl_allocator<T, kernel_type>;
  template <typename T>
  using construct_proxy = util::named_proxy<kernel_type, T, false>;
  template <typename T>
  using construct_iter_proxy = util::named_proxy<kernel_type, T, true>;
};
}

/// \brief Manager class version 0.
///
/// This class is designed to be a delivered class of base_manager with Curiously Recurring Template Pattern (CRTP).
/// Most of important 'public' APIs are inherited from metall::detail::base_manager class.
/// metall::detail::base_manager is an interface class.
/// A method with name "XXX" in base_manager class calls the corresponding method "XXX_impl" in this class.
/// \tparam chunk_no_type
/// The type of chunk number such as uint32_t.
/// \tparam k_chunk_size
/// The chunk size in byte.
/// \tparam kernel_allocator_type
/// The type of the internal allocator
template <typename chunk_no_type = uint32_t,
          std::size_t k_chunk_size = 1 << 21,
          typename kernel_allocator_type = std::allocator<char>>
class manager_v0 : public metall::detail::base_manager<manager_v0<chunk_no_type, k_chunk_size, kernel_allocator_type>,
                                                       detail::manager_type_holder<chunk_no_type,
                                                                                   k_chunk_size,
                                                                                   kernel_allocator_type>> {
 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = manager_v0<chunk_no_type, k_chunk_size, kernel_allocator_type>;
  using type_holder = detail::manager_type_holder<chunk_no_type, k_chunk_size, kernel_allocator_type>;
  using base_type = metall::detail::base_manager<self_type, type_holder>;
  friend base_type;
  using kernel_type = typename type_holder::kernel_type;
  using char_ptr_holder_type = typename kernel_type::char_ptr_holder_type;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using void_pointer = typename type_holder::void_pointer;
  using size_type = typename type_holder::size_type;
  using difference_type = typename type_holder::difference_type;

  template <typename T>
  using allocator_type = typename type_holder::template allocator_type<T>;
  template <typename T>
  using construct_proxy = typename type_holder::template construct_proxy<T>;
  template <typename T>
  using construct_iter_proxy = typename type_holder::template construct_iter_proxy<T>;

  using chunk_number_type = chunk_no_type;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  manager_v0(open_only_t, const char *base_path,
             const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    if (!m_kernel.open(base_path)) {
      std::cerr << "Cannot open " << base_path << std::endl;
      std::abort();
    }
  }

  manager_v0(create_only_t, const char *base_path, const size_type capacity,
             const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.create(base_path, capacity);
  }

  manager_v0(open_or_create_t, const char *base_path, const size_type capacity,
             const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    if (!m_kernel.open(base_path)) {
      m_kernel.create(base_path, capacity);
    }
  }

  manager_v0() = delete;
  ~manager_v0() = default;
  manager_v0(const manager_v0 &) = delete;
  manager_v0(manager_v0 &&) noexcept = default;
  manager_v0 &operator=(const manager_v0 &) = delete;
  manager_v0 &operator=(manager_v0 &&) noexcept = default;

  bool snapshot(char_ptr_holder_type name) {
    return m_kernel.snapshot(name);
  }

  static bool remove_file(const char *base_path) {
    return kernel_type::remove_file(base_path);
  }

  static constexpr size_type chunk_size() {
    return k_chunk_size;
  }

  void profile(const std::string &log_file_name) const {
    m_kernel.profile(log_file_name);
  }

 private:
  /// -------------------------------------------------------------------------------- ///
  /// Private methods (required by the base class)
  /// -------------------------------------------------------------------------------- ///
  template <typename T>
  construct_proxy<T> construct_impl(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, false, true);
  }

  template <typename T>
  construct_proxy<T> find_or_construct_impl(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, true, true);
  }

  template <typename T>
  construct_iter_proxy<T> construct_it_impl(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, false, true);
  }

  template <typename T>
  construct_iter_proxy<T> find_or_construct_it_impl(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, true, true);
  }

  template <typename T>
  std::pair<T *, size_type> find_impl(char_ptr_holder_type name) {
    return m_kernel.template find<T>(name);
  }

  template <typename T>
  bool destroy_impl(char_ptr_holder_type name) {
    return m_kernel.template destroy<T>(name);
  }

  void *allocate_impl(size_type nbytes) {
    return m_kernel.allocate(nbytes);
  }

  void *allocate_aligned_impl(size_type nbytes, size_type alignment) {
    return m_kernel.allocate_aligned(nbytes, alignment);
  }
  // void allocate_many_impl(const std::nothrow_t &tag, size_type elem_bytes, size_type n_elements, multiallocation_chain &chain);

  void deallocate_impl(void *addr) {
    return m_kernel.deallocate(addr);
  }
  // void deallocate_many_impl(multiallocation_chain &chain);

  void sync_impl() {
    m_kernel.sync();
  }

  kernel_type *get_kernel_impl() {
    return &m_kernel;
  }

  template <typename T = void>
  allocator_type<T> get_allocator_impl() {
    metall::detail::g_max_manager_kernel_id = 0;
    *(metall::detail::manager_kernel_table<kernel_type>(typename kernel_type::id_type(0))) = base_type::get_kernel();
    return allocator_type<T>(0); // for now, just support single database mode
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  kernel_type m_kernel;
};
} // namespace v0
} // namespace metall

#endif //METALL_MANAGER_V0_HPP
