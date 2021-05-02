#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Inject DLL attributes etc here. */
#ifdef _WIN32
	#ifdef SYNTHIZER_SHARED
		#ifdef BUILDING_SYNTHIZER
			#define SYZ_CAPI __declspec(dllexport)
		#else
			#define SYZ_CAPI __declspec(dllimport)
		#endif
	#endif
#endif
#ifndef SYZ_CAPI
	#define SYZ_CAPI
#endif

/*
 * Note to maintainers: C API methods that obviously go with  a type live in the main .cpp file for that type, even if it is necessary to introduce a cpp file for that type to hold the C API (i.e. pure abstract classes).
 * 
 * Other methods are in src/c_api.cpp (i.e. generic property setters, initialization, etc.).
 * */

/*
 * A handle to any Synthizer object. Always nonzero.
 * */
typedef unsigned long long syz_Handle;

/*
 * An error. code. Specific error constants TBD. 0 means success.
 * */
typedef int syz_ErrorCode;

/*
 * Event payloads.
 *
 * Note to maintainers: pulling these out to a separate header breaks at least autopxd, and possibly other bindings generators.
 * */
struct syz_EventFinished {
	char unused; // gives the struct a size in C, lets C->C++ not die horribly.
};

struct syz_EventLooped {
	unsigned long long loop_counter;
};

struct syz_Event {
	int type;
	syz_Handle source;
	/**
	 *  * Can be 0. EThe context of the event, if any.
	 * */
	syz_Handle context;
	void *userdata;
	union {
	 struct syz_EventLooped looped;
	 struct syz_EventFinished finished;
	} payload;
	/**
	 * These fields are internal to Synthizer, and should not be touched.
	 * */
	struct {
		unsigned long long flags;
	} _private;
};

/**
 * Free any resources associated with an event.
 * */
SYZ_CAPI void syz_eventDeinit(struct syz_Event *event);

/*
* Configure logging.  Do this before your program calls anything else for reliability.
*
* It is possible to change the level at any time.
*/
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
};

/**
 * Set the default values of the LibraryConfig struct, which are not necessarily all 0.
 * */
SYZ_CAPI void syz_libraryConfigSetDefaults(struct syz_LibraryConfig *config);

/*
 * Library initialization and shutdown.
 * */
SYZ_CAPI syz_ErrorCode syz_initialize(void);	
SYZ_CAPI syz_ErrorCode syz_initializeWithConfig(struct syz_LibraryConfig *config);
SYZ_CAPI syz_ErrorCode syz_shutdown();

/*
 * Get the current error code for this thread. This is like errno, except that functions also return their error codes.
 * 
 * If the last function succeeded, the return value is undefined.
 * */
SYZ_CAPI syz_ErrorCode syz_getLastErrorCode();

/*
 * Get the message associated with the last error for this thread. The pointer is valid only
 * until the next call into Synthizer by this thread.
 * 
 * If the last function succeeded, the return value is undefined.
 * */
SYZ_CAPI const char *syz_getLastErrorMessage();

/**
 * Increment and decrement the reference count on a C handle.
 * 
 * Handles are no longer valid when the reference count reaches 0.  All handles returned from a create function
 * have a reference count of 1.
 * */
SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle);

/**
 * Query the type of a handle. Returns one of the SYZ_OTYPE constants.
 * */
SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle);

/**
 * Userdata support. It is possible to use the following interface to associate arbitrary pointers with Synthizer objects.  If
 * the free_callback is non-NULL, Synthizer will call it on a background thread at the time of the object's death.
 * 
 * An example use for this interface is to associate game objects with Synthizer handles.
 * 
 * Note that there is a latency on the order of 50MS from syz_freeHandle to the userdata freeing callback being called, and latency on the order
 * of 20ms for calling the same callback when userdata is set to a different value. Both of these
 * values are rough worst cases, and it's usually faster. In particular, frequent updates to userdata will effectively batch frees, and the case wherein Synthizer keeps
 * large amounts of userdata alive beyond the next ~100ms or so due to this latency isn't possible.
 * */
SYZ_CAPI syz_ErrorCode syz_getUserdata(void **out, syz_Handle handle);
typedef void syz_UserdataFreeCallback(void *);
SYZ_CAPI syz_ErrorCode syz_setUserdata(syz_Handle handle, void *userdata, syz_UserdataFreeCallback *free_callback);

/**
 * pause/play objects. Supported by everything that produces audio in some fashion: generators, sources, context, etc.
 * */
SYZ_CAPI syz_ErrorCode syz_pause(syz_Handle object);
SYZ_CAPI syz_ErrorCode syz_play(syz_Handle object);

/*
 * Property getters and setters.
 * 
 * See synthizer_constants.h for the constants, and the manual for property definitions.
 * 
 * Note: getters are slow and should only be used for debugging. Synthizer doesn't guarantee that the values returned by getters are what you most
 * immediately set; they are merely some recent value. More advanced library features which introduce deferred setting and transaction-like behavior will break even this guarantee if enabled.
 * If you need to compute values based off what you last set a property to, save it outside Synthizer and do the computation there.
 * */

/* Integer. */
SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value);
/* Double. */
SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value);
/* Object properties. */
SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);
/*
 * double3 and double6 are used for positions and orientations respectively.
 * Double3 is { x, y, z }
 * Double6 is { forward, up }
 * Note that Synthizer's coordinate system is right-handed.
 * */
SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z);
SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2);

/*
 * Biquad properties are biquad filters from the audio eq cookbook.
 * 
 * Note to maintainers: the C API is in src/filter_properties.cpp (except for syz_setBiquad, in c_api.cpp with the other property setters).
 * */

/*
 * Configuration for a filter. Currently this struct should be treated as opaque. It's exposed here
 * in order to allow allocating them on the stack.  Changes to the layout and fields may occur without warning.
 * 
 * To initialize this struct, use one of the syz_BiquadDesignXXX functions, below.
 * */
struct syz_BiquadConfig {
	double b0, b1, b2;
	double a1, a2;
	double gain;
	unsigned char is_wire;
};

SYZ_CAPI syz_ErrorCode syz_getBiquad(struct syz_BiquadConfig *filter, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setBiquad(syz_Handle target, int property, const struct syz_BiquadConfig *filter);

/*
 * Biquad filter design functions.  See the audio eq cookbook in the manual's appendices for the specific mathematical formulas.
 *
 * q is a measure of resonance.  Generally q = 0.5 is a filter that doesn't resonate and q at or above 1 resonates too much to be useful.
 * q = 0.7071135624381276 gives second-order Butterworth lowpass and highpass filters, and is the suggested default.
 * 
 * Synthizer's Nyquist is 22050 HZ.
 * */
SYZ_CAPI syz_ErrorCode syz_biquadDesignIdentity(struct syz_BiquadConfig *filter);
SYZ_CAPI syz_ErrorCode syz_biquadDesignLowpass(struct syz_BiquadConfig *filter, double frequency, double q);
SYZ_CAPI syz_ErrorCode syz_biquadDesignHighpass(struct syz_BiquadConfig *filter, double frequency, double q);

/*
 * bw is the bandwidth between -3 db frequencies.
 * */
SYZ_CAPI syz_ErrorCode syz_biquadDesignBandpass(struct syz_BiquadConfig *filter, double frequency, double bw);

/*
 * Create a context. This represents the audio device itself, and all other Synthizer objects need one.
 * */
SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out);

/*
 * Create a context which doesn't feed an audio device.
 * Currently this is exposed for testing purposes, but will eventually become a stable and useful API.
 * */
SYZ_CAPI syz_ErrorCode syz_createContextHeadless(syz_Handle *out);
/*
 * get a block of audio with 2 channels. This is also 
 * currently only exposed for testing, especially since "block" is not part of the external API and using this wrong is a good way to
 * crash for the time being.
 * */
SYZ_CAPI syz_ErrorCode syz_contextGetBlock(syz_Handle context, float *block);

/**
 * Event support.
 * */

/**
 * Enable events for a context. Once enabled, failure to drain the event queue is effectively a memory leak as the
 * queue will continue to fill forever.
 * 
 * Once enabled, it is not possible to disable events.
 * */
SYZ_CAPI syz_ErrorCode syz_contextEnableEvents(syz_Handle context);

/**
 * Get an event from the queue. If the queue is empty, the event type
 * is SYZ_EVENT_TYPE_INVALID.
 *
 * Flags is one of the `SYZ_EVENT_FLAGS` constants.
 * 
 * Handles returned in an event are guaranteed to be valid until `syz_eventDeinit` is called.
 * This is done by implicitly incrementing and decrementing references, so failure to call `syz_eventDeinit` will leak objects.  If the application wishes
 * to own the events (e.g. because a binding to Synthizer converted the event into a different object), it should set `SYZ_EVENT_FLAG_TAKE_OWNERSHIP`.
 * */
SYZ_CAPI syz_ErrorCode syz_contextGetNextEvent(struct syz_Event *out, syz_Handle context, unsigned long long flags);

/**
 * Stream infrastructure. See the manual for details on these functions.
 * 
 * Most objects provide a convenient shorthand helper for creating a stream handle and passing it into the object.
 * */
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromStreamParams(syz_Handle *out, const char *protocol, const char *path, void *param);
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromMemory(syz_Handle *out, unsigned long long data_len, const char *data);
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromFile(syz_Handle *out, const char *path);

/**
 * Custom streams.
 * 
 * The callbacks here return a non-zero value to indicate an error, and may assign a string whose lifetime shares that
 * of the stream  to `err_msg`.  Currently it isn't possible to extract the code, but the message will be communicated
 * through Synthizer errors where possible and both values may be logged in the background.  Eventually,
 * it will be possible to also extract the error code.
 * */

/**
 * Callbacks for streaming.  Read is mandatory.  If seek is provided, get length must also be set.
 * Synthizer handles position tracking for you internally.  Streams should not
 * change their state out from under Synthizer or be used concurrently by non-Synthizer components.
 * */

/**
 * Read from a stream. Must always write as many bytes as are requested.  Writing
 * less indicates the end of stream.
 * */
typedef int syz_StreamReadCallback(unsigned long long *read, unsigned long long requested, char *destination, void *userdata, const char ** err_msg);

/**
 * Stream seek callback. Should be self-explanatory.
 * */
typedef int syz_StreamSeekCallback(unsigned long long pos, void *userdata, const char **err_msg);

/**
 * Close the stream.
 * */
typedef int syz_StreamCloseCallback(void *userdata, const char **err_msg);

/**
 * Represents a custom stream.  If the length of the stream is unknown, set it to -1.  Note that internally Synthizer
 * treats streams of unknown length as unseekable even if the seek callback is set.
 * */
struct syz_CustomStreamDef {
	syz_StreamReadCallback *read_cb;
	/**
	 * Optional. If unset, this stream doesn't support seeking.
	 * */
	syz_StreamSeekCallback *seek_cb;
	syz_StreamCloseCallback *close_cb;
	long long length;
	void *userdata;
};

/**
 * Get a stream handle from CustomStreamDef.
 * */
SYZ_CAPI syz_ErrorCode syz_streamHandleFromCustomStream(syz_Handle *out, struct syz_CustomStreamDef callbacks);

/**
 * Open a stream by filling out the callbacks struct.
 * */
typedef int syz_StreamOpenCallback(struct syz_CustomStreamDef *callbacks, const char *protocol, const char *path, void *param, void *userdata, const char **err_msg);

/**
 * Register a protocol with the stream handler.
 * 
 * afterword, this can be used as `syz_streamHandleFromStreamParams(&handle, "myprotocol", "mypath", myparam)`.
 * 
 * It is not possible to unregister protocols.
 * */
SYZ_CAPI syz_ErrorCode syz_registerStreamProtocol(const char *protocol, syz_StreamOpenCallback *callback, void *userdata);

/*
 * Create a generator that represents reading from a stream.
 * users who wish to read from files should call syz_createStreamingGeneratorFromFile, which is more future-proof than this API and should not break between major releases.
 * 
 * @param protocol: The protocol. You probably want file.
 * @param path: The path.
 * @param param: an opaque void parameter for the underlying implementation; NULL for built-in protocols.
 * 
 * This will be documented better when there's a manual and more than one type of protocol.
 * 
 * This is a shortcut for creating the stream yourself and adding it to a DecodingGenerator; advanced use cases can use the alternate path for other optimizations in future.
 * 
 * Note to maintainers: lives in src/generators/decoding.cpp, because it's a shortcut for that.
 * */
SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamParams(syz_Handle *out, syz_Handle context, const char *protocol, const char *path, void *param);

/*
 * Same as calling syz_createStreamingGeneratorFromStreamParams with the file protocol. Exists to future-proof the API.
 */
SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromFile(syz_Handle *out, syz_Handle context, const char *path);

/**
 * Create a StreamingGenerator from a stream handle.
 * */
SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamHandle(syz_Handle *out, syz_Handle context, syz_Handle stream);

/*
 * A Buffer is decoded audio data.
 * 
 * This creates one from the 3 streaming parameters.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamParams(syz_Handle *out, const char *protocol, const char *path, void *param);

/**
 * Create a buffer from encoded audio data that's already
 * in memory.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromEncodedData(syz_Handle *out, unsigned long long data_len, const char *data);

/**
 * Create a buffer from a float array in memory.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromFloatArray(syz_Handle *out, unsigned int sr, unsigned int channels, unsigned long long frames, float *data);

/**
 * Create a buffer from a file. Currently equivalent to:
 * syz_createBufferFromStreamParams(&out, "file", "the_path", NULL);
 *
 * Exists to future-proof the API.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromFile(syz_Handle *out, const char *path);

/**
 * create a buffer from a stream handle.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamHandle(syz_Handle *out, syz_Handle stream);

SYZ_CAPI syz_ErrorCode syz_bufferGetChannels(unsigned int *out, syz_Handle buffer);
SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int *out, syz_Handle buffer);
SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSeconds(double *out, syz_Handle buffer);

/*
 * A buffer generator generates audio from a buffer.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context);

/*
 * Add/remove generators from a Source. The Source weak references the generators; they must be kept alive on the external side for the time being.
 * 
 * This will probably change as lifetimes get narrowed down, and as sources are extended with more functionality to manage their generators.
 * 
 * Each generator may only be added to a source once. Duplicate calls are ignored.
 * If a generator isn't on a source, the call silently does nothing.
 * */
SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);

/*
 * Create a DirectSource, which routes audio directly to speakers.
 * This is for music, for example.
 * */
SYZ_CAPI syz_ErrorCode syz_createDirectSource(syz_Handle *out, syz_Handle context);

/*
 * Create a panned source, a source with azimuth/elevation as the underlying panning strategy.
 * 
 * For spatialized audio like games, use Source3D, which has x/y/z and other interesting spatialization properties.
 * */
SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context);

/*
 * A Source3D has x/y/z, and distance model properties.
 * */
SYZ_CAPI syz_ErrorCode syz_createSource3D(syz_Handle *out, syz_Handle context);

SYZ_CAPI syz_ErrorCode syz_createNoiseGenerator(syz_Handle *out, syz_Handle context, unsigned int channels);

/* Initialize with syz_initRouteConfig before using. */
struct syz_RouteConfig {
	float gain;
	float fade_time;
	struct syz_BiquadConfig filter;
};

/*
 * Initialize a syz_routeConfig with default values.
 * 
 * Should be called before using syz_RouteConfig for the first time.  Afterwords, it's fine to just reuse the already-initialized
 * config and change values in it, but some values in the struct need to be nonzero unless explicitly set to 0 by the user.
 * Though the struct is currently simple, it will shortly contain filters which must be properly initialized if audio is to play at all.
 * 
 * The defaults configure a gain of 1 and a fade_time of 0.03 seconds.
 * */
SYZ_CAPI syz_ErrorCode syz_initRouteConfig(struct syz_RouteConfig *cfg);

SYZ_CAPI syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, struct syz_RouteConfig *config);
SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, float fade_out);

SYZ_CAPI syz_ErrorCode syz_effectReset(syz_Handle effect);

SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context);

struct syz_EchoTapConfig {
	float delay;
	float gain_l;
	float gain_r;
};

SYZ_CAPI syz_ErrorCode syz_echoSetTaps(syz_Handle handle, unsigned int n_taps, struct syz_EchoTapConfig *taps);

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context);

#ifdef __cplusplus
}
#endif
