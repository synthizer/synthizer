/**
 * Demonstrate reading raw data into a buffer using some sine generation.
 * */

#include "synthizer.h"
#include "synthizer_constants.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

const unsigned int SR = 44100;
const unsigned int FREQ_LEFT = 200;
const unsigned int FREQ_RIGHT = 400;

#define CHECKED(x) do { \
int ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		goto end; \
	} \
} while(0)

#define PI 3.141592653589793f

void genSine(float frequency, unsigned int length, unsigned int stride, float *out) {
	float omega = 2 * PI * frequency / SR;
	for (unsigned int i = 0; i < length; i++) {
		*(out + i * stride) = sinf(i * omega);
	}
}

int main(int argc, char *argv[]) {
	struct syz_LibraryConfig library_config;
	syz_Handle context = 0, generator = 0, source = 0, buffer = 0;
	/* Used by the CHECKED macro. */
	int ecode = 0;
	float *data = NULL;

	syz_libraryConfigSetDefaults(&library_config);
	library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
	library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
	CHECKED(syz_initializeWithConfig(&library_config));

	CHECKED(syz_createContext(&context, NULL, NULL));
	CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL));
	CHECKED(syz_createDirectSource(&source, context, NULL, NULL));
	CHECKED(syz_sourceAddGenerator(source, generator));

	/* Do one second. */
	data = malloc(SR * 2 * sizeof(float));
	if (data == NULL) {
		printf("Unable to allocate data buffer\n");
		ecode = 1;
		goto end;
	}

	genSine(FREQ_LEFT, SR, 2, data);
	genSine(FREQ_RIGHT, SR, 2, data + 1);
	CHECKED(syz_createBufferFromFloatArray(&buffer, SR, 2, SR, data, NULL, NULL));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
	CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));

	printf("Press any key to quit...\n");
	getchar();

end:
	syz_handleDecRef(context);
	syz_handleDecRef(buffer);
	syz_handleDecRef(generator);
	syz_handleDecRef(buffer);
	free(data);
	return ecode;
}
