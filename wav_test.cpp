#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <memory>

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
#include "synthizer/sources.hpp"

#include "synthizer/generators/decoding.hpp"

using namespace synthizer;

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	logToStdout();
	initializeAudioOutputDevice();

	auto context = std::make_shared<Context>();
	std::shared_ptr<PannedSource> source;


	auto decoder = getDecoderForProtocol("file", argv[1], {});
	auto generator = std::make_shared<DecodingGenerator>();
	generator->setDecoder(decoder);
	generator->setPitchBend(1.0);
	double angle = 0;
	double delta = (360.0/20)*0.02;
	//delta = 0;
	printf("angle delta %f\n", delta);

	source = context->call([&] () {
		auto src = std::make_shared<PannedSource>(context);
		auto base = std::static_pointer_cast<Source>(src);
		context->registerSource(base);
		auto g = std::static_pointer_cast<Generator>(generator);
		src->addGenerator(g);
		return src;
	});
	/* We use references for efficiency, so implicit conversion isn't possible when setting properties. */
	auto source_base = std::static_pointer_cast<BaseObject>(source);

	for(;;) {
		context->setDoubleProperty(source_base, SYZ_PANNED_SOURCE_AZIMUTH, angle);
		angle += delta;
		angle -= (int(angle)/360)*360;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	return 0;
}
