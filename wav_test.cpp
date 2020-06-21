#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <memory>
#include <thread>

#include <cstddef>
#include <cmath>

#include "synthizer.h"
#include "synthizer_constants.h"

#define PI 3.1415926535
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
	syz_Handle context, generator, source, buffer;
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
	CHECKED(syz_createSource3D(&source, context));
	CHECKED(syz_createBufferGenerator(&generator, context));
	CHECKED(syz_createBufferFromStream(&buffer, context, "file", argv[1], ""));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
	CHECKED(syz_sourceAddGenerator(source, generator));

	CHECKED(syz_setD6(context, SYZ_P_ORIENTATION, 0, 1, 0, 0, 0, 1));

	angle = 0;
	delta = (360.0/20)*0.02;
	//delta = 0;
	printf("angle delta %f\n", delta);
	CHECKED(syz_setD(generator, SYZ_P_POSITION, 1.0));

	for(unsigned int i = 0; i < 1000; i++) {
		CHECKED(syz_setD3(source, SYZ_P_POSITION, std::sin(angle*PI/180), std::cos(angle*PI/180), 0.0));
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
