#include <iostream>
#include <metall/metall.hpp>
#include <metall/json/json.hpp>

using metall_value_type =
    metall::json::value<metall::manager::allocator_type<std::byte>>;

int main() {
  std::cout << "Open" << std::endl;
  {
    metall::manager manager(metall::open_read_only, "./test");
    auto *value =
        manager.find<metall_value_type>(metall::unique_instance).first;
    metall::json::pretty_print(std::cout, *value);
  }

  {
    metall::manager manager(metall::open_only, "./test");
    manager.destroy<metall_value_type>(metall::unique_instance);
  }

  return 0;
}
