#include "synthizer/generator.hpp"

#include "synthizer/context.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace synthizer {

void Generator::run(float *output) {
	double new_gain;

	if (this->acquireGain(new_gain) || this->shouldIncorporatePausableGain()) {
		new_gain *= this->getPausableGain();
		this->gain_driver.setValue(this->getContextRaw()->getBlockTime(), new_gain);
	}

	if (this->isPaused()) {
		return;
	}
	this->tickPausable();

	this->generateBlock(output, &this->gain_driver);

	/**
	 * It is necessary to check this every block. If thread a deletes and thread b simultaneously
	 * pauses, then the command for pause can end up in the queue after the command for delete and may still execute because the linger reference keeps the object alive
	 * long enough.
	 * */
	if (this->isPaused()) {
		this->signalLingerStopPoint();
	}
}

bool Generator::wantsLinger() {
	return true;
}

std::optional<double> Generator::startLingering(const std::shared_ptr<CExposable> &ref, double configured_timeout) {
	CExposable::startLingering(ref, configured_timeout);

	/**
	 * If this generator isn't linked to a source, it isn't audible and never can be again.
	 * */
	if (this->use_count.load(std::memory_order_relaxed) == 0) {
		return 0.0;
	}

	if (this->isPaused()) {
		return 0.0;
	}

	if (this->getPauseState() == PauseState::Pausing) {
		return config::BLOCK_SIZE / (double)config::SR;
	}

	return this->startGeneratorLingering();
}

GeneratorRef::GeneratorRef(): ref() {
}

GeneratorRef::GeneratorRef(const std::shared_ptr<Generator> &generator): GeneratorRef(std::weak_ptr<Generator>(generator)) {
}

GeneratorRef::GeneratorRef(const std::weak_ptr<Generator> &weak): ref(weak) {
	auto s = weak.lock();
	if (s) {
		s->use_count.fetch_add(1, std::memory_order_relaxed);
	}
}

GeneratorRef::GeneratorRef(const GeneratorRef &other) {
	this->ref = other.ref;
	auto s = this->ref.lock();
	if (s) {
		s->use_count.fetch_add(1, std::memory_order_relaxed);
	}
}

GeneratorRef::GeneratorRef(GeneratorRef &&other) {
	this->ref = other.ref;
	other.ref = std::weak_ptr<Generator>();
}

GeneratorRef &GeneratorRef::operator=(const GeneratorRef& other) {
	this->ref = other.ref;
	auto s = this->ref.lock();
	if (s) {
		s->use_count.fetch_add(1, std::memory_order_relaxed);
	}
	return *this;
}

GeneratorRef &GeneratorRef::operator=(GeneratorRef &&other) {
	this->decRef();
	this->ref = other.ref;
	other.ref = std::weak_ptr<Generator>();
	return *this;
}

GeneratorRef::~GeneratorRef() {
	this->decRef();
}

void GeneratorRef::decRef() {
	auto s = this->ref.lock();
	if (s) {
		/**
		 * Like weak_ptr and shared_ptr itself, use_count going to 0 may have other consequences. Though using memory_order_release
		 * probably isn't necessary, let's go ahead and actually implement this like weak_ptr to avoid surprises later.
		 * */
		auto uc = s->use_count.fetch_sub(1, std::memory_order_release) - 1;
		if (uc == 0) {
			/**
			 * The generator isn't being used anymore.  Tell it that it can stop lingering.
			 * 
			 * This makes it such that generators which are lingerin imediately die if dropped from 
			 * their sources.
			 * */
			s->signalLingerStopPoint();
		}
	}
}

std::shared_ptr<Generator> GeneratorRef::lock() const {
	return this->ref.lock();
}

bool GeneratorRef::expired()  const {
	return this->ref.expired();
}

}
