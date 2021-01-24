#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <array>
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

struct syz_RouteConfig route_config = { 1.0, 0.01, };

int main(int argc, char *argv[]) {
	syz_Handle context = 0, generator = 0, source = 0, buffer = 0, effect = 0;
	int ecode = 0, ending = 0;
	double angle = 0.0, angle_per_second = 90.0;

	if (argc != 2) {
		printf("Usage: wav_test <path>\n");
		return 1;
	}

	CHECKED(syz_configureLoggingBackend(SYZ_LOGGING_BACKEND_STDERR, nullptr));
	syz_setLogLevel(SYZ_LOG_LEVEL_DEBUG);
	CHECKED(syz_initialize());

	CHECKED(syz_createContext(&context));
	CHECKED(syz_contextEnableEvents(context));
	CHECKED(syz_createSource3D(&source, context));
	CHECKED(syz_createBufferFromStream(&buffer, "file", argv[1], ""));
	CHECKED(syz_createBufferGenerator(&generator, context));
	CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
	CHECKED(syz_sourceAddGenerator(source, generator));

	CHECKED(syz_createGlobalFdnReverb(&effect, context));
	CHECKED(syz_routingConfigRoute(context, source, effect, &route_config));

	//std::this_thread::sleep_for(std::chrono::seconds(2));
	//CHECKED(syz_pause(source));
	//std::this_thread::sleep_for(std::chrono::seconds(5));
	CHECKED(syz_play(source));

	while (true) {
		syz_Event event;

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		CHECKED(syz_setD(effect, SYZ_P_LATE_REFLECTIONS_MODULATION_DEPTH, 0.0));
		angle += 0.01 * angle_per_second;
		angle = fmod(angle, 360.0);
		double rad = angle * PI / 180.0;
		double y = cos(rad);
		double x = sin(rad);
		CHECKED(syz_setD3(source, SYZ_P_POSITION, x, y, 0.0));

		CHECKED(syz_contextGetNextEvent(&event, context));
		if (event.type != SYZ_EVENT_TYPE_INVALID) {
			printf("%u %llu %llu\n", event.type, event.source, event.context);
		}
	}

end:
	ending = 1;
	CHECKED(syz_handleFree(source));
	CHECKED(syz_handleFree(generator));
	CHECKED(syz_handleFree(context));
end2:
	CHECKED(syz_shutdown());
	return ecode;
}
