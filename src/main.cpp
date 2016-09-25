#include <string>
#include <iostream>
#include <type_traits>
#include "./termbox/termbox.h"
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"

struct S_ {
  // static constexpr bool default_ctr = false;

  template <bool B = true, typename = std::enable_if_t<B>>
  S_() { logger() << "constructor"; }

  template <typename... T, typename = std::enable_if_t<sizeof...(T)>>
  S_(T&&... args) {}

  void get() {}
  void get() const {}
};

int main() {
  Logger::init(Logger::createTerminal());
  logger() << std::is_default_constructible<S_>::value;
  S_ s;
  s.get();
  std::cin.get();
}
