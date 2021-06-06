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
}

bool Generator::wantsLinger() {
	return true;
}

std::optional<double> Generator::startLingering(const std::shared_ptr<CExposable> &ref, double configured_timeout) {
	CExposable::startLingering(ref, configured_timeout);
	return std::nullopt;
}

GeneratorRef::GeneratorRef(): ref() {
}

GeneratorRef::GeneratorRef(const std::shared_ptr<Generator> &generator): GeneratorRef(std::weak_ptr(generator)) {
}

GeneratorRef::GeneratorRef(const std::weak_ptr<Generator> &weak): ref(weak) {
	auto s = weak.lock();
	if (s) {
		s->use_count.fetch_add(1, std::memory_order_relaxed);
	}
}

GeneratorRef::~GeneratorRef() {
	auto s = this->ref.lock();
	if (s) {
		/**
		 * Like weak_ptr and shared_ptr itself, use_count going to 0 may have other consequences. Though using memory_order_release
		 * probably isn't necessary, let's go ahead and actually implement this like weak_ptr to avoid surprises later.
		 * */
		s->use_count.fetch_sub(1, std::memory_order_release);
	}
}

std::shared_ptr<Generator> GeneratorRef::lock() const {
	return this->ref.lock();
}

bool GeneratorRef::expired()  const {
	return this->ref.expired();
}

}
