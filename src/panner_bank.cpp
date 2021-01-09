#include "synthizer/panner_bank.hpp"

#include "synthizer/bitset.hpp"
#include "synthizer/config.hpp"
#include "synthizer/hrtf.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/stereo_panner.hpp"

#include "plf_colony.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <tuple>

namespace synthizer {

/*
 * Holds a panner, a bitset for allocating channels to that panner, and a reference count.
 * 
 * This is used with a colony in order to allow us to allocate panners.
 * */
template<typename T>
class PannerBlock {
	public:
	T panner{};
	unsigned int reference_count = 0;
	Bitset<config::MAX_CHANNELS> allocated_lanes;
};

template<typename T>
class ConcretePannerLane: public PannerLane {
	public:
	~ConcretePannerLane() {
		this->block->panner.releaseLane(this->lane);
		this->block->allocated_lanes.set(lane, false);
		this->block->reference_count--;
	}

	void update();
	void setPanningAngles(double azimuth, double elvation);
	void setPanningScalar(double scalar);

	PannerBlock<T> *block;
	unsigned int lane;
};

template<typename T>
void ConcretePannerLane<T>::update() {
	std::tie(this->destination, this->stride) = this->block->panner.getLane(this->lane);
}

template<typename T>
void ConcretePannerLane<T>::setPanningAngles(double azimuth, double elevation) {
	this->block->panner.setPanningAngles(this->lane, azimuth, elevation);
}

template<typename T>
void ConcretePannerLane<T>::setPanningScalar(double scalar) {
	this->block->panner.setPanningScalar(this->lane, scalar);
}

template<typename T>
static std::shared_ptr<PannerLane> allocateLaneHelper(deferred_colony<PannerBlock<T>> &colony) {
	std::shared_ptr<ConcretePannerLane<T>> ret = std::make_shared<ConcretePannerLane<T>>();
	PannerBlock<T> *found = nullptr;
	for(auto &i: colony) {
		if(i.reference_count < i.panner.getLaneCount()) {
			found = &i;
			break;
		}
	}
	if (found == nullptr) {
		found = & (*colony.emplace());
	}
	found->reference_count++;
	unsigned int lane = found->allocated_lanes.getFirstUnsetBit();
	found->allocated_lanes.set(lane, true);
	assert(lane < decltype(found->allocated_lanes)::SIZE);
	ret->lane = lane;
	ret->block = found;
	std::tie(ret->destination, ret->stride) = found->panner.getLane(lane);
	return ret;
}

template<typename T>
static void dropPanners(deferred_colony<PannerBlock<T>> &colony) {
	auto i = colony.begin();
	while (i != colony.end()) {
		if(i->reference_count == 0) {
			i = colony.erase(i);
		} else {
			i++;
		}
	}
}

/*
 * A panner bank for a specific strategy.
 * */
template<typename T>
class StrategyBank {
	public:
	StrategyBank() {
		this->panners.reserve(8);
	}

	std::shared_ptr<PannerLane> allocateLane() {
		return allocateLaneHelper(this->panners);
	}

	void fold(unsigned int channels, unsigned int lanes, unsigned int dest_channels, float *destination) {
		unsigned int needed_channels = std::min(channels, dest_channels);
		unsigned int stride = channels*lanes;
		for(unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
			for(unsigned int l = 0; l < lanes; l++) {
				for(unsigned int c = 0; c < needed_channels; c++) {
					destination[dest_channels*i+c] += this->working_buffer[stride*i+channels*l+c];
				}
			}
		}
	}

	void run(unsigned int channels, float *destination) {
		unsigned int last_channel_count = 0;
		unsigned int last_lane_count = 0;
		bool first = true;
		bool needs_final_fold = false;
		dropPanners(this->panners);

		std::fill(this->working_buffer.begin(), this->working_buffer.end(), 0.0f);

		for(auto &i: this->panners) {
			needs_final_fold = true;
			unsigned int nch = i.panner.getOutputChannelCount();
			unsigned int nl = i.panner.getLaneCount();
			i.panner.run(&working_buffer[0]);
			if (first == false && (nch != last_channel_count || nl != last_lane_count)) {
				this->fold(nch, nl, channels, destination);
				std::fill(this->working_buffer.begin(), this->working_buffer.end(), 0.0f);
				needs_final_fold = false;
			}
			last_channel_count = nch;
			last_lane_count = nl;
			first = false;
		}
		if (needs_final_fold) {
			this->fold(last_channel_count, last_lane_count, channels, destination);
		}
	}

	private:
	deferred_colony<PannerBlock<T>> panners;
	std::array<float, config::BLOCK_SIZE*config::MAX_CHANNELS*config::PANNER_MAX_LANES> working_buffer;
};

class PannerBank: public AbstractPannerBank {
	public:
	void run(unsigned int channels, float *destination) {
		this->hrtf.run(channels, destination);
		this->stereo.run(channels, destination);
	}

	std::shared_ptr<PannerLane> allocateLane(enum SYZ_PANNER_STRATEGY strategy) {
		switch (strategy) {
		case SYZ_PANNER_STRATEGY_HRTF:
			return this->hrtf.allocateLane();
		case SYZ_PANNER_STRATEGY_STEREO:
			return this->stereo.allocateLane();
		default:
			/* Unreachable save for Synthizer bugs. Fail loudly. */
			std::abort();
		}
	}

	private:
	StrategyBank<HrtfPanner> hrtf;
	StrategyBank<StereoPanner> stereo;
};

std::shared_ptr<AbstractPannerBank> createPannerBank() {
	return std::make_shared<PannerBank>();
}

}