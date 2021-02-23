// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_PRIORITY_QUEUE_HPP
#define METALL_CONTAINER_PRIORITY_QUEUE_HPP

#include <queue>
#include <functional>

#include <metall/container/vector.hpp>

namespace metall::container {

template <typename T, typename Container = vector<T>, typename Compare = std::less<typename Container::value_type>>
using priority_queue = std::priority_queue<T, Container>;

}

#endif //METALL_CONTAINER_PRIORITY_QUEUE_HPP
