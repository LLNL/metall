// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_STACK_HPP
#define METALL_CONTAINER_STACK_HPP

#include <stack>

#include <metall/container/deque.hpp>

namespace metall::container {

template <typename T, typename Container = deque<T>>
using stack = std::stack<T, Container>;

}

#endif //METALL_CONTAINER_STACK_HPP
