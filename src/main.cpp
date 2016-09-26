#include <string>
#include <iostream>
#include <type_traits>
#include "./termbox/termbox.h"
#include "./utils/logger.h"
#include "./utils/immutable-struct.hpp"



struct S {
  template <typename T, typename U>
  void test(T t, U v, choice<1>, decltype(t == v)* = nullptr) {
    logger() << "==";
  }

  template <typename T, typename U>
  void test(T t, U v, choice<0>, decltype(t != v)* = nullptr) {
    logger() << "!=";
  }

  template <typename T, typename U>
  void test(T t, U v, otherwise) {
    logger() << "incomparable";
  }

  template <typename T, typename U>
  void run(T t, U u) {
    test(t, u, select_overload);
  }
};

int main() {
  Logger::init(Logger::createTerminal());
  S s;
  s.run(12, 4);
  std::cin.get();
}
