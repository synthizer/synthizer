cmake_minimum_required(VERSION 3.15.0)
project(synthizer VERSION 0.1.0)

include(CTest)
enable_testing()

add_compile_options(-Xclang -fno-caret-diagnostics
	-Xclang -Wno-deprecated-declarations
	-Xclang -Wno-logical-op-parentheses
)

# We always use MultiThreaded, until I can figure out Python setuptools runtime picking options.
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# We must turn on VC++ extended alignment for Windows for plf::colony.
add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)

include(cmake/wdl.txt)

include_directories(
	include
	third_party/miniaudio
	third_party/dr_libs
	third_party/stb
	third_party/cpp11-on-multicore/common
	third_party/wdl
	third_party/plf_colony
	third_party/concurrentqueue
)

set(SYNTHIZER_LIB_TYPE STATIC CACHE STRING "The build type for Synthizer. Either STATIC or SHARED")
add_library(synthizer ${SYNTHIZER_LIB_TYPE}
	src/audio_output.cpp
	src/background_thread.cpp
	src/buffer.cpp
	src/byte_stream.cpp
	src/c_api.cpp
	src/channel_mixing.cpp
	src/context.cpp
	src/decoding.cpp
	src/error.cpp
	src/hrtf.cpp
	src/logging.cpp
	src/memory.cpp
	src/noise_generator.cpp
	src/panner_bank.cpp
	src/router.cpp
	src/single_file_libs.c
	src/spsc_semaphore.cpp
	src/source.cpp
	src/spatialization_math.cpp
	src/stereo_panner.cpp

	src/data/hrtf.cpp

	src/decoders/flac.cpp
	src/decoders/ogg.cpp
	src/decoders/mp3.cpp
	src/decoders/wav.cpp

	src/filters/audio_eq_cookbook.cpp
	src/filters/simple_filters.cpp

	src/generators/buffer.cpp
	src/generators/noise.cpp
	src/generators/streaming.cpp


	src/sources/direct_source.cpp
	src/sources/panned_source.cpp
	src/sources/source3d.cpp

	src/streams/file.cpp
	src/streams/memory.cpp

	$<TARGET_OBJECTS:wdl>
)
target_compile_features(synthizer PUBLIC cxx_std_17)
target_link_libraries(synthizer wdl)
# tells synthizer.h to define SYZ_CAPI for exporting.
target_compile_definitions(synthizer PRIVATE BUILDING_SYNTHIZER)

# For CI artifacts:
if(DEFINED CI_SYNTHIZER_NAME)
set_target_properties(synthizer PROPERTIES OUTPUT_NAME ${CI_SYNTHIZER_NAME})
endif()

add_custom_target(hrtf
python "${CMAKE_SOURCE_DIR}/hrtf_processor/main.py")

# The following won't work on Windows unless compiled against a static build of Synthizer because we don't want to expose the C++ internals from the DLL.
# Since these are just test programs and not actual utilities, disable them on all platforms if the build isn't static.
if("${SYNTHIZER_LIB_TYPE}" STREQUAL "STATIC")
	add_executable(delay_line_test test/delay_line.cpp)
	target_link_libraries(delay_line_test synthizer)

	add_executable(convolve_bench convolve_bench.cpp)
	target_link_libraries(convolve_bench synthizer)

	add_executable(file_test file_test.cpp)
	target_link_libraries(file_test synthizer)

	add_executable(test_filter_repl test/filter_repl.cpp)
	target_link_libraries(test_filter_repl synthizer)

	add_executable(property_write_bench benchmarks/property_write.cpp)
	target_link_libraries(property_write_bench synthizer)

	add_executable(test_random_float test/random_float.cpp)
	target_link_libraries(test_random_float synthizer)

	add_executable(test_noise test/noise.cpp)
	target_link_libraries(test_noise synthizer)

	add_test(NAME delay_line COMMAND delay_line_test)
endif()