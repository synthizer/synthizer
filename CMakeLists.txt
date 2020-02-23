cmake_minimum_required(VERSION 3.13.0)
project(synthizer VERSION 0.1.0)

include(CTest)
enable_testing()


include_directories(include)

add_executable(delay_line_test test/delay_line.cpp)
add_executable(convolve_bench convolve_bench.cpp)

add_test(NAME delay_line COMMAND delay_line_test)
