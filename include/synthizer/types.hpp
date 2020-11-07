#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include "config.hpp"

namespace synthizer {

/*
 * Some basic SFINAE helpers.
 */
template<unsigned int n, typename T=void>
using PowerOfTwo = std::enable_if_t<( ( n & (n-1) ) == 0 && n != 0), T>;
template<unsigned int n, unsigned int mul, typename T=void>
using MultipleOf = typename std::enable_if_t< (n%mul == 0 && n != 0), T>;
// We really need Clang to get concepts, but until then this combines requirements.
template<typename T, typename... T2>
class AllHelper {
	public:
	using type = T;
};

template<typename T, typename... ARGS>
using All = typename AllHelper<T, ARGS...>::type;

/*
 * This is terribly, terribly, terribly slow; use only at compile time.
 * */
constexpr unsigned int nextPowerOfTwo(unsigned int input) {
	unsigned int ret = 1;
	while(ret < input) ret <<= 1;
	return ret;
}

constexpr unsigned int nextMultipleOf(unsigned int value, unsigned int multiplier) {
	if (value % multiplier == 0) return value;
	else return (value / multiplier + 1) * multiplier;
}

}
