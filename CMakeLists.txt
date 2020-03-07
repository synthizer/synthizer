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
	third_party/dr_libs
	third_party/stb
)

add_library(synthizer STATIC
	src/byte_stream.cpp
	src/error.cpp
	src/single_file_libs.c
)
target_compile_features(synthizer PUBLIC cxx_std_17)
target_link_libraries(synthizer wdl)

add_executable(delay_line_test test/delay_line.cpp)
target_link_libraries(delay_line_test synthizer)

add_executable(convolve_bench convolve_bench.cpp)
target_link_libraries(convolve_bench synthizer)

add_executable(miniaudio_test miniaudio_test.cpp)
target_link_libraries(miniaudio_test synthizer)

add_test(NAME delay_line COMMAND delay_line_test)
