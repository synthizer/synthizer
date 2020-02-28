#include "miniaudio.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

const double PI= 3.141592653589793;
double phaseLeft = 0.0, phaseRight = 0.0;
double freqLeft = 99, freqRight = 100;

void data_callback(ma_device* device, void *output, const void *input, ma_uint32 frames) {
	float *output_f = (float*)output;
	for(ma_uint32 i = 0; i < frames; i++) {
		double valLeft = sin(2*PI*phaseLeft);
		phaseLeft += freqLeft/44100;
		phaseLeft = phaseLeft-floor(phaseLeft);
		output_f[i*2] = (float)valLeft;
		double valRight = sin(2*PI*phaseRight);
		phaseRight += freqRight/44100;
		phaseRight = phaseRight-floor(phaseRight);
		output_f[i*2+1] = (float) valRight;
	}
}

int main() {
	ma_result result;
	ma_device_config config;
	ma_device device;

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
