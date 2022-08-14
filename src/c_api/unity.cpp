/*
 *This file is a unity build of the C API.
 *
 * Synthizer tries to be header only to deal with the problem of build times in other systems when there's a lot of
 *translation units, but the C functions will always require some so that they can appear in the library.
 * */

#include "./buffer.cpp"
#include "./core.cpp"
#include "./effects.cpp"
#include "./events.cpp"
#include "./filter_properties.cpp"
#include "./generators.cpp"
#include "./sources.cpp"
#include "./stream_handle.cpp"
