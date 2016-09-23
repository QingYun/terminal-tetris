#pragma once

#include <fstream>
#include <string>
#include <type_traits>

class Logger {
private:
  bool first_;
  std::ofstream& dest_terminal_;
  std::string delimiter_;

public:
  Logger(std::ofstream& dest_terminal, const std::string& delimiter);
  ~Logger();

  template <typename T>
  Logger& operator << (const T& log) {
    if (!first_) {
      dest_terminal_ << delimiter_;
    } else {
      first_ = false;
    }
    dest_terminal_ << log;
    return *this;
  }

  using ManipFunc = std::ostream&(*)(std::ostream&);
  Logger& operator << (ManipFunc manip) {
    manip(dest_terminal_);
    return *this;
  }
};

Logger logger(const std::string& delimiter = "");
