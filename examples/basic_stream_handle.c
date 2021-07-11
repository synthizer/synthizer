/**
 * Demonstrates basic use of a stream handle.
 * 
 * This doesn't show anything particularly special beyond how to create a buffer without going through `syz_createBufferFromFile` and/or
 * `syz_createBufferFromStreamParams`.
 * 
 * Mostly, this exists as a runnable test of the functionality; it's only a one line change from basic file reading.
 * */
#include "synthizer.h"
#include "synthizer_constants.h"

#include <stdio.h>

#define CHECKED(x) do { \
int ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		goto end; \
	} \
} while(0)

int main(int argc, char *argv[]) {
	struct syz_LibraryConfig library_config;
	syz_Handle context = 0, generator = 0, source = 0, buffer = 0, stream = 0;
	/* Used by the CHECKED macro. */
	int ecode = 0;

	if (argc != 2) {
		printf("Usage: buffer_from_memory <path>\n");
		return 1;
	}

	syz_libraryConfigSetDefaults(&library_config);
	library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
	library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
	CHECKED(syz_initializeWithConfig(&library_config));

	CHECKED(syz_createContext(&context, NULL, NULL));
	CHECKED(syz_createBufferGenerator(&generator, context, NULL, NULL));
	CHECKED(syz_createDirectSource(&source, context, NULL, NULL));
	CHECKED(syz_sourceAddGenerator(source, generator));

	CHECKED(syz_createStreamHandleFromStreamParams(&stream, "file", argv[1], NULL, NULL, NULL));
	CHECKED(syz_createBufferFromStreamHandle(&buffer, stream, NULL, NULL));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));

	printf("Press any key to exit...\n");
	getchar();

end:
	syz_handleDecRef(context);
	syz_handleDecRef(generator);
	syz_handleDecRef(buffer);
	syz_handleDecRef(source);
	syz_handleDecRef(stream);
	return ecode;
}
