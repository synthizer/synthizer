/**
 * Demonstrates registering a custom stream protocol.
 * 
 * This just wraps the C file API, but similar techniques can be used
 * to hook into anything else that can be abstracted as callbacks.
 * */
#include "synthizer.h"
#include "synthizer_constants.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECKED(x) do { \
int ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		goto end; \
	} \
} while(0)

int readCallback(unsigned long long *wrote, unsigned long long requested, char * destination,
	void *userdata, const char **err_msg) {
	FILE *f = (FILE *)userdata;
	size_t got = fread(destination, 1, requested, f);
	if (got != requested && ferror(f) != 0) {
		*err_msg = "fread error";
		return 1;
	}
	*wrote = got;
	return 0;
}

int seekCallback(unsigned long long position, void *userdata, const char **err_msg) {
	if (fseek((FILE *)userdata, position, SEEK_SET) != 0) {
		*err_msg = "Unable to seek";
		return 1;
	}
	return 0;
}

int closeCallback(void *userdata, const char **err_msg) {
	fclose((FILE *)userdata);
	return 0;
}

int openCallback(struct syz_CustomStreamDef *def, const char *protocol, const char *path, void *param, void *userdata, const char **err_msg) {
	(void)param;
	(void)userdata;

	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		*err_msg = "Unable to open file";
		return 1;
	}

	/**
	 * Synthizer requires knowing the length for files to be seekable.
	 * */
	if (fseek(f, 0, SEEK_END) != 0) {
		*err_msg = "Unable to seek";
		fclose(f);
		return 1;
	}
	def->length = ftell(f);
	if (def->length < 0) {
		*err_msg = "Unable to ftewll";
		fclose(f);
		return 1;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		*err_msg = "Unable to reset file position";
		fclose(f);
		return 1;
	}

	def->read_cb = readCallback;
	def->close_cb = closeCallback;
	def->seek_cb = seekCallback;
	def->userdata = (void *)f;
	return 0;
}

int main(int argc, char *argv[]) {
	struct syz_LibraryConfig library_config;
	syz_Handle context = 0, generator = 0, source = 0;
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

	CHECKED(syz_registerStreamProtocol("custom", openCallback, NULL));
	CHECKED(syz_createContext(&context, NULL, NULL));
	CHECKED(syz_createDirectSource(&source, context, NULL, NULL));
	CHECKED(syz_createStreamingGeneratorFromStreamParams(&generator, context, "custom", argv[1], NULL, NULL, NULL));
	CHECKED(syz_sourceAddGenerator(source, generator));

	printf("Press any key to quit...\n");
	getchar();

end:
	syz_handleDecRef(context);
	syz_handleDecRef(generator);
	return ecode;
}
