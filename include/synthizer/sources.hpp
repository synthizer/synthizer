#pragma once

#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
#include "synthizer/config.hpp"
#include "synthizer/fade_driver.hpp"
#include "synthizer/faders.hpp"
#include "synthizer/generator.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/pausable.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/routable.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace synthizer {

/* Implementation of a variety of sources. */

class BiquadFilter;
class Context;
class PannerLane;

class Source: public RouteOutput, public Pausable {
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

	bool wantsLinger() override;
	std::optional<double> startLingering(const std::shared_ptr<CExposable> &reference, double configured_timeout) override;

	#define PROPERTY_CLASS Source
	#define PROPERTY_BASE BaseObject
	#define PROPERTY_LIST SOURCE_PROPERTIES
	#include "synthizer/property_impl.hpp"

	protected:
	/*
	 * An internal buffer, which accumulates all generators.
	 * */
	std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> block;

	/*
	 * Fill the internal block, downmixing and/or upmixing generators
	 * to the specified channel count.
	 * 
	 * Also pumps the router, which will feed effects. We do this here so that all source types just work for free.
	 * */
	void fillBlock(unsigned int channels);

	private:
	deferred_vector<GeneratorRef> generators;
	FadeDriver gain_fader = {1.0f, 1};
	/* Used to detect channel changes in fillBlock. */
	unsigned int last_channels = 0;
	std::shared_ptr<BiquadFilter> filter = nullptr, filter_direct = nullptr, filter_effects = nullptr;
	bool is_lingering = false;
	/**
	 * A countdown for lingering. Decremented for every block after there are no generators. When it hits zero,
	 * the source enqueues itself for death.
	 * 
	 * This exists because some source/panner types need a few blocks of audio to finish fading out, as do the
	 * filters. Without it, sources can click on death.
	 * */
	unsigned int linger_countdown = 3;
};

class DirectSource: public Source {
	public:
	DirectSource(std::shared_ptr<Context> ctx): Source(ctx) {}

	int getObjectType() override;
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
	void initInAudioThread() override;

	int getObjectType() override;

	/* For 	Source3D. */
	void setGain3D(double gain);

	void run() override;

	#define PROPERTY_CLASS PannedSource
	#define PROPERTY_LIST PANNED_SOURCE_PROPERTIES
	#define PROPERTY_BASE Source
	#include "synthizer/property_impl.hpp"
	private:
	std::shared_ptr<PannerLane> panner_lane = nullptr;
	double gain_3d = 1.0;
};

class Source3D: public PannedSource {
	public:
	Source3D(std::shared_ptr<Context> context);
	void initInAudioThread() override;

	int getObjectType() override;
	void run() override;

	#define PROPERTY_CLASS Source3D
	#define PROPERTY_BASE PannedSource
	#define PROPERTY_LIST SOURCE3D_PROPERTIES
	#include "synthizer/property_impl.hpp"
};

}
