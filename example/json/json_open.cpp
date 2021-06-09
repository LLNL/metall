#include <iostream>
#include <metall/metall.hpp>
#include <metall/container/experimental/json/json.hpp>

using namespace metall::container::experimental;

int main() {

  std::cout << "Open" << std::endl;

  using metall_value_type = json::value<metall::manager::allocator_type<std::byte>>;
  metall::manager manager(metall::open_read_only, "./test");

  auto *value = manager.find<metall_value_type>(metall::unique_instance).first;
  std::cout << *value << std::endl;

  return 0;
}
