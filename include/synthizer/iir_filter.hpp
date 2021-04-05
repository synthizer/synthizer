#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>

#include "synthizer/config.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/types.hpp"

/*
 * This is an inline, templated IIR Filter runner for a fixed number of channels.
 * */

namespace synthizer {

/* Gives us a point to later introduce vectorization. */
template<typename T, std::size_t n>
class SampleVector {
	public:
	std::array<T, n> values;

	template<typename O>
	SampleVector<T, n> &setScalar(O other) {
		std::fill(this->values.begin(), this->values.end(), (T)other);
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &copyFrom(SampleVector<O, n> &other) {
		std::copy(other.values.begin(), other.values.end(), this->values.begin());
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &load(O *from) {
		std::copy(from, from+n, &this->values[0]);
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &store(O *out) {
		std::copy(this->values.begin(), this->values.end(), out);
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &mul(const SampleVector<O, n> &other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] *= other.values[i];
		}
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &mulScalar(O other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] *= other;
		}
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &add(const SampleVector<O, n> &other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] += other.values[i];
		}
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &addScalar(O other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] += other;
		}
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &sub(const SampleVector<O, n> &other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] -= other.values[i];
		}
		return *this;
	}

	template<typename O>
	SampleVector<T, n> &subScalar(O other) {
		for(unsigned int i = 0; i < n; i++) {
			this->values[i] -= other;
		}
	}
};

template<std::size_t lanes, std::size_t num, std::size_t den>
class IIRFilter {
	public:
	static const unsigned int HISTORY_SIZE = nextPowerOfTwo(num > den ? num : den);
	std::array<SampleVector<double, lanes>, HISTORY_SIZE> history;
	unsigned int counter = 0; // for the ringbuffer.

	// Filter parameters.
	std::array<SampleVector<float, lanes>, num> numerator;
	std::array<SampleVector<double, lanes>, den-1> denominator;
	SampleVector<float, lanes> gain;

	IIRFilter() {
		reset();
		identity();
	}

	void identity() {
		for(unsigned int i = 0; i < this->numerator.size(); i++) {
			this->numerator[i].setScalar(0.0);
		}
		for(unsigned int i = 0; i < this->denominator.size(); i++) {
			this->denominator[i].setScalar(0.0);
		}
		this->gain.setScalar(1.0);
		this->numerator[0].setScalar(1.0f);
	}

	void reset() {
		for(auto &h: this->history) {
			h.setScalar(0.0);
		}
	}

	template<std::size_t nn, std::size_t nd>
	void setParametersForLane(unsigned int l, const IIRFilterDef<nn, nd> &params) {
		static_assert(nn <= num && nd <= den, "IIRFilter instance is not big enough to contain this filter.");
		assert(l < lanes);
		for(unsigned int i = 0; i < num; i++) {
			this->numerator[i].values[l] = i < nn ? params.num_coefs[i] : 0.0;
		}
		for(unsigned int i = 0; i < den - 1; i++) {
			/* Recall that denominators are minus the first sample. This expression is weird because nd - 1 can wrap when nd == 0. */
			this->denominator[i].values[l] = i + 1 < nd ? params.den_coefs[i] : 0.0;
		}
		this->gain.values[l] = params.gain;
	}

	template<std::size_t nn, std::size_t nd>
	void setParameters(const IIRFilterDef<nn, nd> &params) {
		for(unsigned int i = 0; i < lanes; i++) setParametersForLane(i, params);
	}

	void tick(float *in, float *out) {
		// First apply the IIR, which needs to be done as double.
		SampleVector<double, lanes> working_recursive;
		working_recursive.load(in)
			.mul(this->gain);
		for(unsigned int i = 0; i < this->denominator.size(); i++) {
			auto h = this->history[(this->counter-i)%this->history.size()];
			h.mul(this->denominator[i]);
			working_recursive.sub(h);
		}
		this->counter = (this->counter + 1) % this->history.size();
		this->history[this->counter] = working_recursive;

		// and now the output, which is convolution.
		SampleVector<double, lanes> sum;
		sum.setScalar(0.0);
		for(unsigned int i = 0; i < this->numerator.size(); i++) {
			auto h = this->history[(this->counter - i )%this->history.size()];
			h.mul(this->numerator[i]);
			sum.add(h);
		}
		sum.store(out);
	}
};

/*
 * Make an IIR filter of l lanes from the provided definition.
 * 
 * Typical usage: makeIIRFilter<2>(myDefinitionHere)
 * */
template<std::size_t l, std::size_t n, std::size_t d>
IIRFilter<l, n, d> makeIIRFilter(const IIRFilterDef<n, d> &def) {
	IIRFilter<l, n, d> ret{};
	ret.setParameters(def);
	return ret;
}

}
