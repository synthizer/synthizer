cmake_minimum_required(VERSION 3.13.0)
project(synthizer VERSION 0.1.0)

include(CTest)
enable_testing()

add_compile_options(-fno-caret-diagnostics
	-Wno-deprecated-declarations
	-Wno-logical-op-parentheses
)

include(cmake/wdl.txt)

include_directories(
	include
	third_party/miniaudio
)

add_executable(delay_line_test test/delay_line.cpp)
add_executable(convolve_bench convolve_bench.cpp src/miniaudio_impl.c)
add_executable(miniaudio_test miniaudio_test.cpp src/miniaudio_impl.c)
target_link_libraries(miniaudio_test wdl)

add_test(NAME delay_line COMMAND delay_line_test)
