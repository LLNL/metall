// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This file contains some code from Boost.Interprocess.
// Here is the original license:
// (C) Copyright Ion Gaztanaga 2005-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef METALL_UTILITY_DETAIL_NAMED_PROXY_HPP
#define METALL_UTILITY_DETAIL_NAMED_PROXY_HPP

#include <boost/interprocess/detail/named_proxy.hpp>

/// \namespace metall::mtlldetail
/// \brief Namespace for implementation details
namespace metall::mtlldetail {

template <typename T, bool is_iterator, typename... Args>
using CtorArgN =
    boost::interprocess::ipcdetail::CtorArgN<T, is_iterator, Args...>;

/// \brief Proxy class that implements named allocation syntax.
/// \tparam segment_manager segment manager to construct the object
/// \tparam T type of object to build
/// \tparam is_iterator passing parameters are normal object or iterators?
template <typename segment_manager, typename T, bool is_iterator>
class named_proxy {
 public:
  using char_type = typename segment_manager::char_type;

  named_proxy(segment_manager *mngr, const char_type *name, bool find,
              bool dothrow)
      : m_name(name),
        m_mngr(mngr),
        m_num(1),
        m_find(find),
        m_dothrow(dothrow) {}

  template <typename... Args>
  T *operator()(Args &&...args) const {
    CtorArgN<T, is_iterator, Args...> ctor_obj(std::forward<Args>(args)...);
    return m_mngr->template generic_construct<T>(
        m_name, m_num, m_find, m_dothrow, ctor_obj);
  }

  // This operator allows --> named_new("Name")[3]; <-- syntax
  const named_proxy &operator[](const std::size_t num) const {
    m_num *= num;
    return *this;
  }

 private:
  const char_type *const m_name;
  segment_manager *const m_mngr;
  mutable std::size_t m_num;
  const bool m_find;
  const bool m_dothrow;
};

}  // namespace metall::mtlldetail

#endif  // METALL_UTILITY_DETAIL_NAMED_PROXY_HPP
