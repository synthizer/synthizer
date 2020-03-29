#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "synthizer/audio_output.hpp"
#include "synthizer/byte_stream.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/audio_ring.hpp"

using namespace synthizer;


	int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	logToStdout();
	initializeAudioOutputDevice();
	auto output = createAudioOutput();
	auto decoder = getDecoderForProtocol("file", argv[1], {});
	bool started = false;

	for(;;) {
		auto b = output->beginWrite();
		decoder->writeSamplesInterleaved(config::BLOCK_SIZE, b);
		output->endWrite();
	}

	return 0;
}
