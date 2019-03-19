// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>

struct dummy {
  dummy(int _a, int _b) {}
  int a;
  int b;
};

int main()
{
  using manager = metall::v0::manager<uint32_t, 1 << 21, 8>;
  std::remove("name");

  {
    manager manager(metall::create_only, "name", 1 << 25);
  }

  {
    manager manager(metall::open_only, "name");
  }

  {
    manager manager(metall::open_or_create, "name", 1 << 25);

    {
      manager.construct<dummy>("obje1")(10, 20);
      manager.find_or_construct<dummy>("obje1")(10, 20);
    }

    {
      std::vector<int> init_a{1, 2, 3, 4, 5};
      std::vector<int> init_b{6, 7, 8, 9, 10};
      manager.construct_it<dummy>("obje3")[5](init_a.begin(), init_b.begin());
      manager.find_or_construct_it<dummy>("obje3")[5](init_a.begin(), init_b.begin());
      manager.destroy<dummy>("obje3");
    }

    {
      manager.find<dummy>("obje1");
      manager.destroy<dummy>("obje1");
    }

    {
      auto a1 = manager.allocate(16);
      auto a2 = manager.allocate_aligned(16, 1024);
      manager.deallocate(a1);
      manager.deallocate(a2);
    }

    {
      manager.sync();
    }

    {
      // auto kernel = manager.get_kernel(); // this is used in stl allocator
      auto allocator = manager.get_allocator<dummy>();
      std::vector<int, manager::allocator_type<int>> vec(allocator);
      vec.push_back(10);
    }
  }

  return 0;
}