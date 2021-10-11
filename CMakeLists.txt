cmake_minimum_required(VERSION 3.15.0)
project(synthizer VERSION 0.1.0 LANGUAGES C CXX)

# Do this first; some things will quietly pick it up and it's
# really hard to tell which.
set(CMAKE_CXX_STANDARD 17)

# Synthizer version.
add_compile_definitions(SYZ_MAJOR=0 SYZ_MINOR=11 SYZ_PATCH=1)

include(CTest)
include(CheckCXXSourceRuns)
enable_testing()

find_package(Threads)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-Xclang -fno-caret-diagnostics
    -Xclang -Wno-deprecated-declarations
    -Xclang -Wno-logical-op-parentheses
  )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(
    # Lets us use Clang pragmas.
    /wd4068
    # Lets us use non-MSVC attributes.
    /wd5030
    # allows implicit casts to smaller types. We need this because we can't i.e. std::copy(double, float) without
    # hitting it.
    /wd4244
    /wd4267
    # Lets property generation work, namely property_impl.hpp
    /Zc:preprocessor
    # And now we need to silence a winbase.h warning because it's got a macro expanding to undefined somehow. Noe that
    # this isn't our code:
    /wd5105
    # Not all compilers give us constexpr if, but MSVC likes to warn us when we can use it.
    /wd4127
    # Documented here: https://devblogs.microsoft.com/cppblog/broken-warnings-theory/
    # By using this and putting Synthizer dependency headers behind <>, we can suppress warnings in dependencies.
    /experimental:external
    /external:W0
    /external:anglebrackets
    # Apparently we have to globally disable unreachable code warnings; MSVC isn't letting us do it in a
    # targeted fashion.
    /wd4702
  )
endif()

# Inspired by the module in https://github.com/vector-of-bool/CMakeCM/
# Determine whether or not filesystem is non-experimental and doesn't need -lstdc++fs
#
# This makes things work on GCC < 9.0
#
# This is one of the places which requires the global CXX_STANDARD be set.
check_cxx_source_runs([[
  #include <cstdio>
  #include <cstdlib>
  #include <filesystem>

  int main() {
    auto cwd = std::filesystem::current_path();
    std::printf("%s", cwd.c_str());
    return 0;
  }
]] FILESYSTEM_OK)

if(NOT FILESYSTEM_OK)
  message(WARNING "C++17 filesystem not found or not functional on this platform")
else()
  message(STATUS "Found C++17 filesystem support")
endif()

include(cmake/wdl.txt)

include_directories(SYSTEM
  include
  third_party/miniaudio
  third_party/dr_libs
  third_party/stb
  third_party/cpp11-on-multicore/common
  third_party/wdl
  third_party/plf_colony
  third_party/concurrentqueue
  third_party/pdqsort
)

# Needs to be factored out so that -Wall doesn't apply to third-party deps.
add_library(synthizer_single_file_libs OBJECT
  src/single_file_libs.c
)
set_property(TARGET synthizer_single_file_libs PROPERTY POSITION_INDEPENDENT_CODE ON)

set(SYNTHIZER_LIB_TYPE STATIC CACHE STRING "The build type for Synthizer. Either STATIC or SHARED")
add_library(synthizer ${SYNTHIZER_LIB_TYPE}
  src/audio_output.cpp
  src/automation_batch.cpp
  src/background_thread.cpp
  src/base_object.cpp
  src/biquad.cpp
  src/block_buffer_cache.cpp
  src/buffer.cpp
  src/byte_stream.cpp
  src/c_api.cpp
  src/channel_mixing.cpp
  src/context.cpp
  src/decoding.cpp
  src/error.cpp
  src/event_timeline.cpp
  src/events.cpp
  src/filter_properties.cpp
  src/generator.cpp
  src/hrtf.cpp
  src/logging.cpp
  src/memory.cpp
  src/noise_generator.cpp
  src/panner_bank.cpp
  src/pausable.cpp
  src/prime_helpers.cpp
  src/property_internals.cpp
  src/routable.cpp
  src/router.cpp
  src/shared_object.cpp
  src/source.cpp
  src/spatialization_math.cpp
  src/spsc_semaphore.cpp
  src/stereo_panner.cpp
  src/stream_handle.cpp
  src/version.cpp

  src/data/arrays.cpp
  src/data/hrtf.cpp

  src/decoders/flac.cpp
  src/decoders/libsndfile.cpp
  src/decoders/mp3.cpp
  src/decoders/raw_decoder.cpp
  src/decoders/wav.cpp

  src/effects/echo.cpp
  src/effects/effects.cpp
  src/effects/fdn_reverb.cpp

  src/filters/audio_eq_cookbook.cpp
  src/filters/simple_filters.cpp

  src/generators/buffer.cpp
  src/generators/noise.cpp
  src/generators/streaming.cpp

  src/sources/angular_panned_source.cpp
  src/sources/direct_source.cpp
  src/sources/panned_source.cpp
  src/sources/scalar_panned_source.cpp
  src/sources/source3d.cpp

  src/streams/custom_stream.cpp
  src/streams/file.cpp
  src/streams/memory_stream.cpp

  $<TARGET_OBJECTS:wdl>
  $<TARGET_OBJECTS:synthizer_single_file_libs>
)
target_compile_features(synthizer PUBLIC cxx_std_17)
target_link_libraries(synthizer wdl Threads::Threads ${CMAKE_DL_LIBS})

target_compile_definitions(synthizer PRIVATE BUILDING_SYNTHIZER)
# tells synthizer.h to define SYZ_CAPI for exporting shared object/dll symbols.
# On windows this is dllexport.
if("${SYNTHIZER_LIB_TYPE}" STREQUAL "SHARED")
  target_compile_definitions(synthizer PRIVATE SYNTHIZER_SHARED)
endif()
if(${FILESYSTEM_OK})
  target_compile_definitions(synthizer PRIVATE SYZ_USE_FILESYSTEM)
endif()

if(MSVC)
  target_compile_options(synthizer PRIVATE /W4 /WX)
else()
  target_compile_options(synthizer PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

set_property(TARGET synthizer PROPERTY POSITION_INDEPENDENT_CODE ON)

# For CI artifacts:
if(DEFINED CI_SYNTHIZER_NAME)
  set_target_properties(synthizer PROPERTIES OUTPUT_NAME ${CI_SYNTHIZER_NAME})
endif()

add_custom_target(data
python "${CMAKE_SOURCE_DIR}/data_processor/main.py")

macro(example NAME EXT)
  add_executable(${NAME} ./examples/${NAME}.${EXT})
  target_link_libraries(${NAME} synthizer)
endmacro()

example(automation_circle cpp)
example(basic_stream_handle c)
example(buffer_from_memory c)
example(buffer_from_raw_data c)
example(custom_stream c)
example(events cpp)
example(load_libsndfile c)
example(play_note c)
example(print_version c)
example(scalar_panned_source cpp)
example(simple_automation c)

# The following won't work on Windows unless compiled against a static build of Synthizer because we don't want to expose the C++ internals from the DLL.
# Since these are just test programs and not actual utilities, disable them on all platforms if the build isn't static.
if("${SYNTHIZER_LIB_TYPE}" STREQUAL "STATIC")
  add_executable(delay_line_test test/delay_line.cpp)
  target_link_libraries(delay_line_test synthizer)

  add_executable(file_test file_test.cpp)
  target_link_libraries(file_test synthizer)

  add_executable(test_filter_repl test/filter_repl.cpp)
  target_link_libraries(test_filter_repl synthizer)

  add_executable(property_write_bench benchmarks/property_write.cpp)
  target_link_libraries(property_write_bench synthizer)

  add_executable(decoding_bench benchmarks/decoding.cpp)
  target_link_libraries(decoding_bench synthizer)

  add_executable(test_random_float test/random_float.cpp)
  target_link_libraries(test_random_float synthizer)

  add_executable(test_noise test/noise.cpp)
  target_link_libraries(test_noise synthizer)

  add_executable(test_latch test/latch.cpp)
  target_link_libraries(test_latch synthizer)

  add_executable(test_verify_properties test/verify_properties.cpp)
  target_link_libraries(test_verify_properties synthizer)

  # can't be an actual unit test until we have silent contexts.
  add_executable(test_effect_connection test/effect_connection.cpp)
  target_link_libraries(test_effect_connection synthizer)

  add_executable(test_generation_thread test/generation_thread.cpp)
  target_link_libraries(test_generation_thread synthizer)

  add_executable(test_double_refcount test/double_refcount.c)
  target_link_libraries(test_double_refcount synthizer)

  add_executable(test_property_automation_timeline test/property_automation_timeline.cpp)
  target_link_libraries(test_property_automation_timeline synthizer)

  add_test(NAME delay_line COMMAND delay_line_test)
  add_test(NAME latch COMMAND test_latch)
  add_test(NAME verify_properties COMMAND test_verify_properties)
  add_test(NAME generation_thread COMMAND test_generation_thread)
  add_test(NAME double_refcount COMMAND test_double_refcount)
  add_test(NAME property_automation_timeline COMMAND test_property_automation_timeline)
endif()

install(
  TARGETS synthizer
  LIBRARY DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION  "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )
