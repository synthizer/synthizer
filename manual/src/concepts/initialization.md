# Logging, Initialization, and Shutdown

The following excerpts from `synthizer.h` specify the loggin and initialization API. Explanation follows:

```
typedef ... syz_Handle;

enum SYZ_LOGGING_BACKEND {
	SYZ_LOGGING_BACKEND_STDERR = 0,
};

SYZ_CAPI syz_ErrorCode syz_configureLoggingBackend(enum SYZ_LOGGING_BACKEND backend, void *param);

enum SYZ_LOG_LEVEL {
	SYZ_LOG_LEVEL_ERROR = 0,
	SYZ_LOG_LEVEL_WARN = 10,
	SYZ_LOG_LEVEL_INFO = 20,
	SYZ_LOG_LEVEL_DEBUG = 30,
};

SYZ_CAPI void syz_setLogLevel(enum SYZ_LOG_LEVEL level);

SYZ_CAPI syz_ErrorCode syz_initialize();
SYZ_CAPI syz_ErrorCode syz_shutdown();
```

Synthizer supports logging backends, which should be configured before calling `syz_initialize()` and never thereafter.  The `param` is backend specific, and unused for `stderr` (the only supported option). Log levels work exactly how one would expect and can be changed at any time.

`syz_initialize() and `syz_shutdown()` initialize and shut the library down respectively.  Calls to these nest; every `syz_initialize` should be matched with a `syz_shutdown`.  This is supported so that multiple dependencies to a program can initialize Synthizer without conflict, but centralizing initialization and only doing it once is *strongly* encouraged.