cmake_minimum_required(VERSION 3.15.0)
project(synthizer VERSION 0.1.0 LANGUAGES C CXX)

set(BENCHMARK_ENABLE_TESTING OFF)

OPTION(SYZ_INTEGRATING "Set to ON to avoid linking examples etc. when integrating into a language's bindings" OFF)

# Do this first; some things will quietly pick it up and it's
# really hard to tell which.
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(deps)

# Synthizer version.
add_compile_definitions(SYZ_MAJOR=0 SYZ_MINOR=11 SYZ_PATCH=7)

include(CTest)
include(CheckCXXSourceRuns)
enable_testing()

find_package(Threads)

set(SYZ_DEPS
  boost_partial
  concurrentqueue
  cpp11-on-multicore
  dr_libs
  hedley
  miniaudio
  pdqsort
  # wdl is special because it's an object library.
  #wdl
)

if(NOT "${SYZ_INTEGRATING}")
  set(SYZ_TEST_DEPS catch2 benchmark)
endif()

FetchContent_MakeAvailable(${SYZ_DEPS} ${SYZ_TEST_DEPS})
FetchContent_MakeAvailable(wdl)

include_directories(include)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Xclang -fno-caret-diagnostics
    -Wno-deprecated-declarations
    -Wno-logical-op-parentheses
    -Wno-unknown-pragmas
    # We have warning pragmas that are GCC specific.
    -Wno-unknown-warning-option
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
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  ADD_COMPILE_OPTIONS(-Wno-unknown-pragmas)
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



# Needs to be factored out so that -Wall doesn't apply to third-party deps.
add_library(synthizer_single_file_libs OBJECT
  src/single_file_libs.c
)
set_property(TARGET synthizer_single_file_libs PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(synthizer_single_file_libs dr_libs miniaudio)

set(SYNTHIZER_LIB_TYPE STATIC CACHE STRING "The build type for Synthizer. Either STATIC or SHARED")
add_library(synthizer ${SYNTHIZER_LIB_TYPE}
  src/audio_output.cpp
  src/base_object.cpp
  src/byte_stream.cpp
  # includes our C API source files into a unity build.
  src/c_api/unity.cpp
  src/context.cpp
  src/decoding.cpp
  src/error.cpp
  src/event_timeline.cpp
  src/events.cpp
  src/generator.cpp
  src/logging.cpp
  src/memory.cpp
  src/pausable.cpp
  src/property_internals.cpp
  src/routable.cpp
  src/router.cpp
  src/shared_object.cpp
  src/data/arrays.cpp
  src/data/hrtf.cpp
  src/streams/custom_stream.cpp
  src/streams/file.cpp
  src/streams/memory_stream.cpp

  $<TARGET_OBJECTS:wdl_objlib>
  $<TARGET_OBJECTS:synthizer_single_file_libs>
)
target_compile_features(synthizer PUBLIC cxx_std_17)
target_link_libraries(synthizer ${SYZ_DEPS} Threads::Threads ${CMAKE_DL_LIBS})
target_include_directories(synthizer PRIVATE $<TARGET_PROPERTY:wdl_objlib,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(synthizer PRIVATE BUILDING_SYNTHIZER WDL_RESAMPLE_TYPE=float)
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
  target_compile_options(synthizer PRIVATE -Wall -Wextra -Werror)
endif()

set_property(TARGET synthizer PROPERTY POSITION_INDEPENDENT_CODE ON)

if (WIN32)
  # Don't let MSVC define min and max from windows.h.
  target_compile_definitions(synthizer PRIVATE NOMINMAX)
endif()

# For CI artifacts:
if(DEFINED CI_SYNTHIZER_NAME)
  set_target_properties(synthizer PROPERTIES OUTPUT_NAME ${CI_SYNTHIZER_NAME})
endif()

add_custom_target(data
python "${CMAKE_SOURCE_DIR}/data_processor/main.py")

# Not linked to C examples by default. on Linux.
find_library(MATH_LIBRARY m)

function(example NAME EXT)
  if (NOT "${SYZ_INTEGRATING}")
    add_executable(${NAME} ./examples/${NAME}.${EXT})
    target_link_libraries(${NAME} synthizer)
    if(MATH_LIBRARY)
      target_link_libraries(${NAME} ${MATH_LIBRARY})
    endif()
  endif()
endfunction()

example(automation_circle cpp)
example(basic_stream_handle c)
example(buffer_from_memory c)
example(buffer_from_raw_data c)
example(custom_stream c)
example(events cpp)
example(fast_sine_bank cpp)
example(load_libsndfile c)
example(play_note c)
example(print_version c)
example(scalar_panned_source cpp)
example(simple_automation c)
# The following won't work on Windows unless compiled against a static build of Synthizer because we don't want to
# expose the C++ internals from the DLL. Since these are just test programs and not actual utilities, disable them on
# all platforms if the build isn't static.
if(("${SYNTHIZER_LIB_TYPE}" STREQUAL "STATIC") AND (NOT "${SYZ_INTEGRATING}"))
  add_executable(file_test file_test.cpp)
  target_link_libraries(file_test synthizer)

  add_executable(test_filter_repl test/interactive/filter_repl.cpp)
  target_link_libraries(test_filter_repl synthizer)

  add_executable(decoding_bench benchmarks/decoding.cpp)
  target_link_libraries(decoding_bench synthizer)

  add_executable(test_noise test/interactive/noise.cpp)
  target_link_libraries(test_noise synthizer)

  add_executable(test_seeking test/interactive/seeking.cpp)
  target_link_libraries(test_seeking synthizer)

  add_executable(test_fast_sine_accuracy test/interactive/fast_sine_accuracy.cpp)
  target_link_libraries(test_fast_sine_accuracy synthizer)

  FetchContent_MakeAvailable(benchmark catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  INCLUDE(Catch)

  # Set up benchmarks for things we can benchmark.
  #
  # Unfortunately Synthizer's state is too global for us to do this reliably until we pull the math out to a library,
  # but we can at least group some things under Google's benchmarking library which is better than nothing.

  # Apparently we have to do this globally because CMake won't let us set a define for a subdirectory. Without it
  # google/benchmark wants gtest.
  set(BENCHMARK_ENABLE_TESTING OFF)

  add_executable(gbench
    benchmarks/gbench/block_buffer_cache.cpp
    benchmarks/gbench/hrtf.cpp
    benchmarks/gbench/main.cpp
    benchmarks/gbench/property_write.cpp
    benchmarks/gbench/standard_setup.cpp
  )
  target_link_libraries(gbench synthizer benchmark::benchmark)
  set_property(TARGET gbench PROPERTY CXX_STANDARD 17)

  FetchContent_MakeAvailable(benchmark catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
  INCLUDE(Catch)

  add_executable(tests
    test/block_delay_line.cpp
    test/buffer_generator.cpp
    test/delay_line.cpp
    test/double_refcount.cpp
    test/effect_connection.cpp
    test/generation_thread.cpp
    test/latch.cpp
    test/main.cpp
    test/math.cpp
    test/mod_pointer.cpp
    test/property_automation_timeline.cpp
    test/random_float.cpp
    test/sse2_horizontal_sum.cpp
    test/verify_properties.cpp
  )
  target_link_libraries(tests PRIVATE Catch2::Catch2 synthizer)
  target_include_directories(tests PRIVATE $<TARGET_PROPERTY:wdl_objlib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(tests PRIVATE WDL_RESAMPLE_TYPE=float)
  if (WIN32)
    # Don't let MSVC define min and max from windows.h.
    target_compile_definitions(tests PRIVATE NOMINMAX)
  endif()


  catch_discover_tests(tests)

endif()

install(
  TARGETS synthizer
  LIBRARY DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION  "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )
