cmake_minimum_required(VERSION 3.15.0)
project(synthizer VERSION 0.1.0 LANGUAGES C CXX)

find_package(Threads)

include(CTest)
enable_testing()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-Xclang -fno-caret-diagnostics
		-Xclang -Wno-deprecated-declarations
		-Xclang -Wno-logical-op-parentheses
	)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(
		# Lets us use Clang pragmas.
		/wd4068
		# allows implicit casts to smaller types. We need this because we can't i.e. std::copy(double, float) without hitting it.
		/wd4244
		/wd4267
		# Lets property generation work, namely property_impl.hpp
		/Zc:preprocessor
		# And now we need to silence a winbase.h warning because it's got a macro expanding to undefined somehow. Noe that this isn't our code:
		/wd5105
	)
endif()

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
	src/block_buffer_cache.cpp
	src/buffer.cpp
	src/byte_stream.cpp
	src/c_api.cpp
	src/channel_mixing.cpp
	src/context.cpp
	src/decoding.cpp
	src/effects/echo.cpp
	src/effects/effects.cpp
	src/effects/fdn_reverb.cpp
	src/error.cpp
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
	src/single_file_libs.c
	src/spsc_semaphore.cpp
	src/source.cpp
	src/spatialization_math.cpp
	src/stereo_panner.cpp

	src/data/arrays.cpp
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
target_link_libraries(synthizer wdl Threads::Threads ${CMAKE_DL_LIBS})
target_compile_definitions(synthizer PRIVATE BUILDING_SYNTHIZER)
# tells synthizer.h to define SYZ_CAPI for exporting shared object/dll symbols.
# On windows this is dllexport.
if("${SYNTHIZER_LIB_TYPE}" STREQUAL "SHARED")
	target_compile_definitions(synthizer PRIVATE SYNTHIZER_SHARED)
endif()


# For CI artifacts:
if(DEFINED CI_SYNTHIZER_NAME)
set_target_properties(synthizer PROPERTIES OUTPUT_NAME ${CI_SYNTHIZER_NAME})
endif()

add_custom_target(data
python "${CMAKE_SOURCE_DIR}/data_processor/main.py")

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

	add_test(NAME delay_line COMMAND delay_line_test)
	add_test(NAME latch COMMAND test_latch)
	add_test(NAME verify_properties COMMAND test_verify_properties)
endif()

install(
	TARGETS synthizer
	LIBRARY DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
	ARCHIVE DESTINATION  "${CMAKE_INSTALL_LIBDIR}"
	RUNTIME DESTINATION  "${CMAKE_INSTALL_BINDIR}"
	INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
	)
