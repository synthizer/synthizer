#include "miniaudio.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "synthizer/byte_stream.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/logging.hpp"

using namespace synthizer;

const double PI= 3.141592653589793;
double phaseLeft = 0.0, phaseRight = 0.0;
double freqLeft = 99, freqRight = 100;
std::shared_ptr<AudioDecoder> decoder;

void data_callback(ma_device* device, void *output, const void *input, ma_uint32 frames) {
	float *output_f = (float*)output;
	auto got = decoder->writeSamplesInterleaved(frames, output_f, 2);
	std::fill(output_f+got, output_f+frames, 0.0f);
}

int main(int argc, char *argv[]) {
	ma_result result;
	ma_device_config config;
	ma_device device;

	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	logToStdout();
	decoder = getDecoderForProtocol("file", argv[1], {});

	config = ma_device_config_init(ma_device_type_playback);
	config.playback.format = ma_format_f32;
	config.playback.channels = 2;
	config.sampleRate = 44100;
	config.dataCallback = data_callback;

	if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
		printf("Failed to open playback device.\n");
		return 1;
	}

	if(ma_device_start(&device) != MA_SUCCESS) {
		printf("Couldn't start device\n");
		ma_device_uninit(&device);
		return 1;
	}

	printf("Enter to quit");
	getchar();
	return 0;
}
