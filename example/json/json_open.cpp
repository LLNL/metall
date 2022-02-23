#include <iostream>
#include <metall/metall.hpp>
#include <metall/container/experimental/json/json.hpp>

using namespace metall::container::experimental;

using metall_value_type = json::value<metall::manager::allocator_type<std::byte>>;

int main() {

  std::cout << "Open" << std::endl;
  {
    metall::manager manager(metall::open_read_only, "/tmp/dir/test_0");
    auto *value = manager.find<metall_value_type>(metall::unique_instance).first;
    json::pretty_print(std::cout, *value);
  }

  {
    metall::manager manager(metall::open_only, "/tmp/dir/test_0");
    manager.destroy<metall_value_type>(metall::unique_instance);
  }

  return 0;
}
