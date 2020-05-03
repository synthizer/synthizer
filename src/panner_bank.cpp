#include "synthizer/panner_bank.hpp"

#include "synthizer/bitset.hpp"
#include "synthizer/config.hpp"

#include "plf_colony.h"

#include <cassert>
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
	Bitset<config::MAX_CHANNELS> allocated_channels;
};

template<typename T>
class ConcretePannerLane: PannerLane {
	public:
	~ConcretePannerLane() {
		this->block->bitset.set(lane, false);
		this->block->reference_count--;
	}

	PannerBlock<T> *block;
	unsigned int lane;
};

template<typename T>
static std::shared_ptr<PannerLane> allocateLaneHelper(plf::colony<PannerBlock<T>> &colony) {
	std::shared_ptr<ConcretePannerLane> ret = std::make_shared<ConcretePannerLane();
	PannerBlock<T> *found = nullptr;
	for(auto &i: colony) {
		if(i.reference_count < i.getLanes()) {
			found = &i;
			break;
		}
	}
	if (found == nullptr) {
		found = & (*colony.emplace());
	}
	found->reference_count++;
	unsigned int lane = found->bitset.getFirstUnsetBit();
	assert(lane < decltype(found->bitset)::SIZE);
	ret->lane = lane;
	ret->block = found;
	std::tie(ret->destination, ret->stride) = found->panner.getLane(lane);
	found->panner.recycleLane(lane);
	return ret;
}

template<typename T>
static void dropPanners(plf::colony<PannerBlock<T>> &colony) {
	for(auto i = colony.begin(); i != colony.end(); i++) {
		if(i->reference_count == 0) colony.erase(i);
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

	void run(AudioSample *destination) {
		dropPanners(this->panners);
		for(auto &i: this->panners) {
			i.run(destination);
		}
	}

	private:
	plf::colony<PannerBlock<T>> panners;
};

class PannerBank: public AbstractPannerbank {
	public:
	void run(AudioSample *destination) {
		hrtf.run(destination);
	}

	std::shared_ptr<PannerLane> allocateLane(enum SYZ_PANNER_STRATEGIES strategy) {
		assert(strategy == SYZ_PANNER_STRATEGY_HRTF);
		return this->hrtf.allocateLane();
	}

	private:
	StrategyBank<HrtfPanner> hrtf;
};

std::shared_ptr<AbstractPannerbank> makePannerBank() {
	return std::make_shared<PannerBank>();
}

}