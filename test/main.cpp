#include "synthizer.h"

#include <catch2/catch_all.hpp>

#include <iostream>

int main(int argc, char *argv[]) {
  if (syz_initialize() != 0) {
    std::cerr << "Unable to initialize synthizer" << std::endl;
    return 1;
  }

  int result = Catch::Session().run(argc, argv);
  syz_shutdown();
  return result;
}
