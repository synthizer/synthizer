#pragma once

#include <array>

namespace synthizer {

/*
 * We sometimes need static arrays of unknown size, i.e. for the decoder list,
 * and C++ isn't smart enough to infer it for us save if we use experimental/too recent features.
 * 
 * This function does that so that we don't have another bug around manually specifying array sice.
 * 
 * Usage is makeStaticArray(TypeName{...}, TypeName{...}, TypeName{...});
 * */
template<typename T, typename... REST>
auto makeStaticArray(T &&first, REST&&... rest) -> std::array<T, sizeof...(rest) + 1> {
	return { first, rest... };
}

}