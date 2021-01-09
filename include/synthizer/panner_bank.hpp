#pragma once

#include "synthizer_constants.h"
#include "synthizer/types.hpp"

#include <tuple>
#include <memory>

namespace synthizer {

/*
 * This is the panner bank infrastructure. Panner banks hold multiple copies of panners, and run them in parallel.
 * 
 * */

/*
 * A lane of a panner. Stride is the distance between the elements, and destination the place to copy to.
 * 
 * It is necessary to call update on every tick from somewhere in order to refresh stride and destination.
 * */
class PannerLane {
	public:
	virtual ~PannerLane() {}
	virtual void update() = 0;

	unsigned int stride = 1;
	float *destination = nullptr;

	virtual void setPanningAngles(double azimuth, double elvation) = 0;
	virtual void setPanningScalar(double scalar) = 0;
};

/*
 * Abstract interface for panners.
 * */
class AbstractPanner {
	public:
	virtual ~AbstractPanner() {}

	virtual unsigned int getOutputChannelCount() = 0;
	virtual unsigned int getLaneCount() = 0;
	/* writes lanes*channels. Should add to the output. The pannerBank will collapse/remix as necessary. */
	virtual void run(float *output) = 0;
	/* (destination, stride). Allocation and freeing is handled elsewhere. */
	virtual std::tuple<float *, unsigned int> getLane(unsigned int lane) = 0;
	/* Called when we're going to reuse a lane. */
	virtual void releaseLane(unsigned int lane) = 0;

	/*
	 * These set panning, either as azimuth and elevation, or as -1 to 1.
	 * The -1 to 1 interface is for stereo panning; HRTF and surround sound fake it out.
	 * 
	 * Azimuth and elevation are degrees, matching the HRTF dataset.
	 * */
	virtual void setPanningAngles(unsigned int lane, double azimuth, double elevation) = 0;
	/* -1 is left, 1 is right. */
	virtual void setPanningScalar(unsigned int lane, double scalar) = 0;
};

/*
 * The PannerBank
 * 
 * The implementation of this is hidden in the .cpp file.
 * */
class AbstractPannerBank {
	public:
	virtual ~AbstractPannerBank() {}

	/* Will output the same channels as the final device in the end. Today, outputs 2 channels until context infrastructure fully exists. */
	virtual void run(unsigned int channels, float *destination) = 0;
	virtual std::shared_ptr<PannerLane> allocateLane(enum SYZ_PANNER_STRATEGY strategy) = 0;
};

std::shared_ptr<AbstractPannerBank> createPannerBank();

}