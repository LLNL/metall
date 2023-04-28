// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/metall.hpp>

using T = int;

// Just a one way to extract the type of the attributed object directory
// iterator
using iterator_t = metall::manager::const_named_iterator;

int main() {
  metall::manager manager(metall::create_only, "/tmp/dir");
  T* obj = manager.construct<T>("obj")();

  [[maybe_unused]] bool flag;
  [[maybe_unused]] std::size_t n;
  [[maybe_unused]] std::string str;
  [[maybe_unused]] iterator_t itr;

  // Accessing object attributes via metall::manager object
  {
    manager.get_instance_name<T>(obj);
    manager.get_instance_kind<T>(obj);
    n = manager.get_instance_length<T>(obj);
    manager.get_instance_description<T>(obj, &str);
    manager.set_instance_description<T>(obj, "foo");
    manager.is_instance_type<T>(obj);
    itr = manager.named_begin();
    itr = manager.named_end();
    itr = manager.unique_begin();
    itr = manager.unique_end();
    itr = manager.anonymous_begin();
    itr = manager.anonymous_end();
  }

  // Attributed Object Directory Accessor
  {
    // Named object
    auto asn = metall::manager::access_named_object_attribute("/dir");
    flag = asn.good();
    n = asn.num_objects();
    n = asn.count("obj");
    itr = asn.find("name");
    itr = asn.begin();
    itr = asn.end();
    flag = asn.set_description("obj", "foo");
    flag = asn.set_description(itr, "foo");

    // Unique object
    auto asu = metall::manager::access_unique_object_attribute("/dir");
    flag = asu.good();
    n = asu.num_objects();
    n = asu.count(typeid(T).name());
    n = asu.count<T>(metall::unique_instance);
    itr = asu.find(typeid(T).name());
    itr = asu.find<T>(metall::unique_instance);
    itr = asu.begin();
    itr = asu.end();
    asu.set_description(typeid(T).name(), "foo");
    asu.set_description<T>(metall::unique_instance, "foo");
    asu.set_description(itr, "foo");

    // Anonymous object
    auto asa = metall::manager::access_anonymous_object_attribute("/dir");
    flag = asa.good();
    n = asa.num_objects();
    itr = asa.begin();
    itr = asa.end();
    asa.set_description(itr, "foo");
  }

  // Attributed Object Directory Iterator
  {
    itr->name();
    n = itr->length();
    itr->type_id();
    itr->description();
    flag = itr->is_type<T>();
  }

  return 0;
}