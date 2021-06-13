/*
 * this is the C API for filter properties.\
 * */

#include "synthizer.h"

#include <tuple>

#include "synthizer/c_api.hpp"
#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"

using namespace synthizer;

/*
 * The library's internal filter definition is very different from the external; convert
 * from one to the other.
 * */
syz_BiquadConfig convertBiquadDef(const BiquadFilterDef &def) {
	struct syz_BiquadConfig out{};
	out._b0 = def.num_coefs[0];
	out._b1 = def.num_coefs[1];
	out._b2 = def.num_coefs[2];
	out._a1 = def.den_coefs[0];
	out._a2 = def.den_coefs[1];
	out._gain = def.gain;
	out._is_wire = 0;
	return out;
}

SYZ_CAPI syz_ErrorCode syz_biquadDesignIdentity(struct syz_BiquadConfig *filter) {
	SYZ_PROLOGUE
	*filter = syz_BiquadConfig();
	filter->_b0 = 1.0;
	filter->_gain = 1.0;
	filter->_is_wire = 1;
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_biquadDesignLowpass(struct syz_BiquadConfig *filter, double frequency, double q) {
	SYZ_PROLOGUE
	*filter = convertBiquadDef(designAudioEqLowpass(frequency / config::SR, q));
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_biquadDesignHighpass(struct syz_BiquadConfig *filter, double frequency, double q) {
	SYZ_PROLOGUE
	*filter = convertBiquadDef(designAudioEqHighpass(frequency / config::SR, q));
	return 0;
	SYZ_EPILOGUE
}

SYZ_CAPI syz_ErrorCode syz_biquadDesignBandpass(struct syz_BiquadConfig *filter, double frequency, double bw) {
	SYZ_PROLOGUE
	*filter = convertBiquadDef(designAudioEqBandpass(frequency / config::SR, bw));
	return 0;
	SYZ_EPILOGUE
}
