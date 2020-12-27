#include "synthizer/generator.hpp"

#include "synthizer/context.hpp"

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

}
