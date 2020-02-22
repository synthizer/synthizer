#pragma once

#include <type_traits>

namespace synthizer {

/*
 * Basic sample type.
 */
using AudioSample = float;

/*
 * Some types, i.e. SIMD extensions, may not have a simple way to be zero initialized.
 * In particular, Clang and GCC don't document the interaction with C++ initializer lists.
 * Since the only operation that varies is the zeroing operation for most of this library,w e deal wit that here.
 */
template<typename T>
T zeroValue();

template<>
float zeroValue<float>() { return 0.0f; }
template<>
double zeroValue<double>() { return 0.0; }

/*
 * Some basi SFINAE helpers.
 */
template<unsigned int n, typename T=void>
using PowerOfTwo = std::enable_if_t<( ( n & (n-1) != 0) && n != 0), T>;
template<unsigned int n, unsigned int mul, typename T=void>
using MultipleOf = std::enable_if_t<n%mul == 0 && n != 0, T>;
// We really need Clang to get concepts, but until then this combines requirements.
template<typename T, typename... ARGS>
using All = std::enable_if_t<std::void_t<...ARGS>, T>;

}
