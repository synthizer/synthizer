# Logging, Initialization, and Shutdown

The following excerpts from `synthizer.h` specify the loggin and initialization
API. Explanation follows:

```
enum SYZ_LOGGING_BACKEND {
    SYZ_LOGGING_BACKEND_NONE,
    SYZ_LOGGING_BACKEND_STDERR,
};

enum SYZ_LOG_LEVEL {
    SYZ_LOG_LEVEL_ERROR = 0,
    SYZ_LOG_LEVEL_WARN = 10,
    SYZ_LOG_LEVEL_INFO = 20,
    SYZ_LOG_LEVEL_DEBUG = 30,
};

struct syz_LibraryConfig {
    unsigned int log_level;
    unsigned int logging_backend;
    const char *libsndfile_path;
};

SYZ_CAPI void syz_libraryConfigSetDefaults(struct syz_LibraryConfig *config);

SYZ_CAPI syz_ErrorCode syz_initialize(void);    
SYZ_CAPI syz_ErrorCode syz_initializeWithConfig(struct syz_LibraryConfig *config);
SYZ_CAPI syz_ErrorCode syz_shutdown();
```

Synthizer can be initialized in two ways.  The simplest is `syz_initialize`
which will use reasonable library defaults for most apps.  The second is
(excluding error checking):

```
struct syz_LibraryConfig cfg;

syz_libraryConfigSetDefaults(&config)l;
syz_initializeWithConfig(&config);
```

In particular, the latter approach allows for enabling logging and loading
Libsndfile.

Currently Synthizer can only log to stderr and logging is slow enough that it
shouldn't be enabled in production.  It mostly exists for debugging.  In future
these restrictions will be lifted.

For more information on Libsndfile support, see [the dedicated
chapter](./libsndfile.md).
