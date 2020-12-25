#pragma once

#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/faders.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/routable.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <memory>
#include <vector>

namespace synthizer {

/* Implementation of a variety of sources. */

class Context;
class Generator;
class PannerLane;

class Source: public RouteOutput {
	public:
	Source(std::shared_ptr<Context> ctx): RouteOutput(ctx) {
		this->setGain(1.0);
	}

	virtual ~Source() {}

	/* Should write to appropriate places in the context on its own. */
	virtual void run() = 0;

	/* Weak add and/or remove a generator from this source. */
	virtual void addGenerator(std::shared_ptr<Generator> &gen);
	virtual void removeGenerator(std::shared_ptr<Generator> &gen);
	bool hasGenerator(std::shared_ptr<Generator> &generator);

	#define PROPERTY_CLASS Source
	#define PROPERTY_BASE BaseObject
	#define PROPERTY_LIST SOURCE_PROPERTIES
	#include "synthizer/property_impl.hpp"

	protected:
	/*
	 * An internal buffer, which accumulates all generators.
	 * */
	alignas(config::ALIGNMENT) std::array<float, config::BLOCK_SIZE * config::ALIGNMENT> block;

	/*
	 * Fill the internal block, downmixing and/or upmixing generators
	 * to the specified channel count.
	 * 
	 * Also pumps the router, which will feed effects. We do this here so that all source types just work for free.
	 * */
	void fillBlock(unsigned int channels);

	private:
	deferred_vector<std::weak_ptr<Generator>> generators;
	FadeDriver gain_fader = {1.0f, 1};
};

class DirectSource: public Source {
	public:
	DirectSource(std::shared_ptr<Context> ctx): Source(ctx) {}

	void run() override;
};

/*
 * A PannedSource is controlled through azimuth/elevation or panning scalar plus a gain factor.
 * 
 * Outputs are mono.
 * */
class PannedSource: public Source {
	public:
	PannedSource(std::shared_ptr<Context> context);

	/* For 	Source3D. */
	void setGain3D(double gain);

	void run() override;

	#define PROPERTY_CLASS PannedSource
	#define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
	#define PROPERTY_BASE Source
	#include "synthizer/property_impl.hpp"
	private:
	std::shared_ptr<PannerLane> panner_lane;
	double gain_3d = 1.0;
};

class Source3D: public PannedSource {
	public:
	Source3D(std::shared_ptr<Context> context);
	void initInAudioThread() override;

	void run() override;

	#define PROPERTY_CLASS Source3D
	#define PROPERTY_BASE PannedSource
	#define PROPERTY_LIST SOURCE3D_PROPERTIES
	#include "synthizer/property_impl.hpp"
};

}
