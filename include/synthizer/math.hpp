#pragma once

#include <cmath>

namespace synthizer {
const double PI = 3.141592653589793;
const float PIF = (float)PI;

/* WebAudio definition, also field quantity. */
inline double dbToGain(double db) {
	return std::pow(10.0, db/20.0);
}
inline double gainToDb(double gain) {
	return 20 * std::log10(gain);
}

template<typename T>
T clamp(T v, T min, T max) {
	if (v < min) return min;
	if (v > max) return max;
	return v;
}

}