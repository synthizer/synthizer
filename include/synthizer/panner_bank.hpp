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
 * */
class PannerLane {
	public:
	virtual ~PannerLane() {}

	unsigned int stride = 1;
	AudioSample *destination = nullptr;
};

/*
 * Abstract interface for panners.
 * */
class AbstractPanner {
	virtual unsigned int getOutputChannels() = 0;
	virtual unsigned int getLaneCount() = 0;
	/* writes lanes*channels. Should add to the output. The pannerBank will collapse/remix as necessary. */
	virtual void runPanning(AudioSample *output) = 0;
	/* (destination, stride). Allocation and freeing is handled elsewhere. */
	virtual std::tuple<AudioSample *, unsigned int> getLane(unsigned int lane) = 0;

	/*
	 * These set panning, either as azimuth and elevation, or as -1 to 1.
	 * The -1 to 1 interface is for stereo panning; HRTF and surround sound fake it out.
	 * 
	 * Azimuth and elevation are degrees, matching the HRTF dataset.
	 * */
	virtual void setPanningAngles(double azimuth, double elevation) = 0;
	/* -1 is left, 1 is right. */
	virtual void setPanningScalar(double scalar) = 0;
};

/*
 * The PannerBank
 * 
 * The implementation of this is hidden in the .cpp file.
 * */
class AbstractPannerbank {
	virtual std::shared_ptr<PannerLane> allocateLane(enum SYZ_PANNER_STRATEGIES strategy) = 0;
};

std::shared_ptr<AbstractPannerbank> makePannerBank();

}