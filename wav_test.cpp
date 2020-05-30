#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <memory>

#include <cstddef>

#include "synthizer.h"
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

#define CHECKED(x) do { \
auto ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		if (ending == 0) goto end; \
		else goto end2; \
	} \
} while(0)

int main(int argc, char *argv[]) {
	syz_Handle context, generator, source;
	int ecode = 0, ending = 0;
	double angle, delta;

	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	CHECKED(syz_configureLoggingBackend(SYZ_LOGGING_BACKEND_STDERR, nullptr));
	syz_setLogLevel(SYZ_LOG_LEVEL_DEBUG);
	CHECKED(syz_initialize());

	CHECKED(syz_createContext(&context));
	CHECKED(syz_createPannedSource(&source, context));
	CHECKED(syz_createStreamingGenerator(&generator, context, "file", argv[1], ""));
	CHECKED(syz_sourceAddGenerator(source, generator));

	angle = 0;
	delta = (360.0/20)*0.02;
	//delta = 0;
	printf("angle delta %f\n", delta);

	for(unsigned int i = 0; i < 100; i++) {
		CHECKED(syz_setD(source, SYZ_PANNED_SOURCE_AZIMUTH, angle));
		angle += delta;
		angle -= (int(angle)/360)*360;
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

end:
	ending = 1;
	CHECKED(syz_handleDecRef(source));
	CHECKED(syz_handleDecRef(generator));
	CHECKED(syz_handleDecRef(context));
end2:
	CHECKED(syz_shutdown());
	return ecode;
}
