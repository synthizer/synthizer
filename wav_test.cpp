#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include <cstddef>

#include "synthizer_constants.h"

#include "synthizer/audio_output.hpp"
#include "synthizer/byte_stream.hpp"
#include "synthizer/context.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/downsampler.hpp"
#include "synthizer/filter_design.hpp"
#include "synthizer/hrtf.hpp"
#include "synthizer/iir_filter.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/math.hpp"

#include "synthizer/generators/decoding.hpp"

using namespace synthizer;

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	logToStdout();
	initializeAudioOutputDevice();

	Context *context = new Context();
	std::shared_ptr<PannerLane> lane;

	auto decoder = getDecoderForProtocol("file", argv[1], {});
	DecodingGenerator generator{};
	generator.setDecoder(decoder);
	generator.setPitchBend(1.0);
	double angle = 0;
	double delta = (360.0/20)*0.02;
	//delta = 0;
	printf("angle delta %f\n", delta);

	auto alloc_panner_inv = WaitableInvokable([&] () {
		return context->allocateSourcePannerLane(SYZ_PANNER_STRATEGY_HRTF);
	});
	context->enqueueInvokable(&alloc_panner_inv);
	lane = alloc_panner_inv.wait();

	for(;;) {
		auto inv = WaitableInvokable([&] () {
			float wav[config::BLOCK_SIZE*config::MAX_CHANNELS] = {0.0f};
			unsigned int nch = generator.getChannels();
			generator.generateBlock(wav);
			lane->update();
			lane->setPanningAngles(angle, 0.0);
			for(unsigned int i = 0; i < config::BLOCK_SIZE; i++) {
				lane->destination[i*lane->stride] = 0.9*(0.5*wav[i*nch]+0.5*wav[i*nch+1]);
			}

			angle += delta;
			angle -= (int(angle)/360)*360;
		});
		context->enqueueInvokable(&inv);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	return 0;
}
