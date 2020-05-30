#pragma once

#include "synthizer_constants.h"

#include "synthizer/base_object.hpp"
#include "synthizer/property_internals.hpp"

#include <array>
#include <memory>
#include <vector>

namespace synthizer {

/* Implementation of a variety of sources. */

class Context;
class Generator;
class PannerLane;

class Source: public BaseObject {
	public:
	Source(std::shared_ptr<Context> ctx): BaseObject(ctx) {}
	virtual ~Source() {}

	/* Should write to appropriate places in the context on its own. */
	virtual void run() = 0;

	/* Weak add and/or remove a generator from this source. */
	virtual void addGenerator(std::shared_ptr<Generator> &gen);
	virtual void removeGenerator(std::shared_ptr<Generator> &gen);
	bool hasGenerator(std::shared_ptr<Generator> &generator);

	protected:
	std::vector<std::weak_ptr<Generator>> generators;
};

/*
 * A PannedSource is controlled through azimuth/elevation or panning scalar plus a gain factor.
 * 
 * Outputs are mono.
 * */
class PannedSource: public Source {
	public:
	PannedSource(std::shared_ptr<Context> context);

	double getAzimuth();
	void setAzimuth(double azimuth);
	double getElevation();
	void setElevation(double elevation);
	double getPanningScalar();
	void setPanningScalar(double scalar);
	int getPannerStrategy();
	void setPannerStrategy(int strategy);
	double getGain();
	void setGain(double gain);

	void run() override;

	PROPERTY_METHODS;
	private:
	enum SYZ_PANNER_STRATEGIES panner_strategy = SYZ_PANNER_STRATEGY_HRTF;
	std::shared_ptr<PannerLane> panner_lane;
	double azimuth = 0.0, elevation = 0.0, panning_scalar = 0.5, gain = 1.0;
	bool needs_panner_set = true;
	/* If true, the last thing set was scalar and we use that; otherwise use azimuth/elevation. */
	bool is_scalar_panning = false;
	/* Set to false to make the audio thread reallocate the lane on strategy changes. */
	bool valid_lane = false;
};

}