/**
 * Demonstrate simple use of the automation API by synthesizing a tone and playing beeps using an ADSR envelope.
 * */
#include "synthizer.h"
#include "synthizer_constants.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned int SR = 9000;
static const double FREQ = 300;
static const unsigned int HARMONICS = 3;
static const double PI = 3.141592653589793;

static const struct syz_AutomationPoint POINTS[] = {
	{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.0, { 0.0 } },
	{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.1, { 0.8 } },
	{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.4, { 0.5 } },
	{ SYZ_INTERPOLATION_TYPE_LINEAR, 0.9, { 0.01 } },
	{ SYZ_INTERPOLATION_TYPE_LINEAR, 1.0, { 0.0 } },
};

float *computeTriangle() {
	float *ret = calloc(SR, sizeof(float));
	assert(ret != NULL);
	for (unsigned int i = 0; i < SR; i++) {
		double acc = 0.0;
		for (unsigned int h = 0; h < HARMONICS; h++) {
			double hfreq = FREQ * h;
			double hsin = 1.0;
			if (i % 2 == 1) {
				hsin = -1.0;
			}
			double hmul = 1.0 / (h * h);
			acc += hsin * hmul * sin(2 * PI * FREQ * i / (double)SR);
		}
		acc *= 8 / (PI * PI);
		ret[i] = acc;
	}
	return ret;
}

#define CHECKED(x) do { \
int ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		goto end; \
	} \
} while(0)

int main() {
	int ecode =0;
	float *triangle = NULL;
	syz_Handle context = 0, buffer = 0, generator = 0, source = 0, timeline = 0;

	CHECKED(syz_initialize());
	CHECKED(syz_createContext(&context, NULL, NULL));
	CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL));
	CHECKED(syz_createDirectSource(&source, context, NULL, NULL));

	triangle = computeTriangle();
	CHECKED(syz_createBufferFromFloatArray(&buffer, SR, 1, SR, triangle, NULL, NULL));
	free(triangle);
	triangle = NULL;

	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer, NULL, NULL));
	CHECKED(syz_createAutomationTimeline(&timeline, sizeof(POINTS)/sizeof(POINTS[0]), POINTS, 0, NULL, NULL));
	CHECKED(syz_automateD(generator, SYZ_P_GAIN, timeline));
	CHECKED(syz_sourceAddGenerator(source, generator));

	printf("Press any key to exit\n");
	getchar();
end:
	syz_handleDecRef(source);
	syz_handleDecRef(buffer);
	syz_handleDecRef(timeline);
	syz_handleDecRef(generator);
	syz_handleDecRef(context);
	syz_shutdown();
	free(triangle);
	return ecode;
}
