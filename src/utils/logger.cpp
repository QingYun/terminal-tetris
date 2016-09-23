#include <memory>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "./logger.h"

Logger::Logger(std::ofstream& dest_terminal, const std::string& delimiter)
  : dest_terminal_(dest_terminal), delimiter_(delimiter) {
    first_ = true;
  }

Logger::~Logger() {
  dest_terminal_ << std::endl;
}

Logger logger(const std::string& delimiter) {
  static std::ofstream dest_terminal;
  if (!dest_terminal.is_open()) {
    std::string cmd = "x-terminal-emulator -e cat ";
    std::unique_ptr<char> name(tempnam(nullptr, nullptr));
    cmd += name.get();
    mkfifo(name.get(), 0777);
    if (fork() == 0) {
      system(cmd.c_str());
      exit(0);
    }

    dest_terminal.open(name.get());
  }

  return Logger(dest_terminal, delimiter);
}
