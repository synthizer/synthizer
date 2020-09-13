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
	CHECKED(syz_createBufferFromStream(&buffer, "file", argv[1], ""));
	CHECKED(syz_createBufferGenerator(&generator, context));
	//CHECKED(syz_setI(generator, SYZ_P_LOOPING, 1));
	CHECKED(syz_setD(generator, SYZ_P_POSITION, 10.0));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));
	CHECKED(syz_sourceAddGenerator(source, generator));


	std::this_thread::sleep_for(std::chrono::seconds(20));

end:
	ending = 1;
	CHECKED(syz_handleFree(source));
	CHECKED(syz_handleFree(generator));
	CHECKED(syz_handleFree(context));
end2:
	CHECKED(syz_shutdown());
	return ecode;
}
