/**
 * Shows how to use Libsndfile by playing an audio file.
 * In addition to the first argument (the path to a file), the second argument is a path to libsndfile.
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

	if (argc != 3) {
		printf("Usage: load_libsndfile <path> <libsndfile_path>\n");
		return 1;
	}

	syz_libraryConfigSetDefaults(&library_config);
	library_config.log_level = SYZ_LOG_LEVEL_DEBUG;
	library_config.logging_backend = SYZ_LOGGING_BACKEND_STDERR;
	library_config.libsndfile_path = argv[2];
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
