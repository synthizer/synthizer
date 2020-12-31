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
* Configure logging.  Do this before your program calls anything else for reliability.
*
* It is possible to change the level at any time.
*/
enum SYZ_LOGGING_BACKEND {
	SYZ_LOGGING_BACKEND_STDERR = 0,
};

/**
 * The parameter is specific to the backend:
 *
 * - For STDERR, ignored.
 */
SYZ_CAPI syz_ErrorCode syz_configureLoggingBackend(enum SYZ_LOGGING_BACKEND backend, void *param);

enum SYZ_LOG_LEVEL {
	SYZ_LOG_LEVEL_ERROR = 0,
	SYZ_LOG_LEVEL_WARN = 10,
	SYZ_LOG_LEVEL_INFO = 20,
	SYZ_LOG_LEVEL_DEBUG = 30,
};

SYZ_CAPI void syz_setLogLevel(enum SYZ_LOG_LEVEL level);

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

/*
 * Library initialization and shutdown.
 * */
SYZ_CAPI syz_ErrorCode syz_initialize();
SYZ_CAPI syz_ErrorCode syz_shutdown();
/**
 * Free a C handle.
 * */
SYZ_CAPI syz_ErrorCode syz_handleFree(syz_Handle handle);

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
 * immediately set; they are merely some recent value. More advanced library features which introduce deferred setting and transsaction-like behavior will break even this guarantee if enabled.
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

/*
 * Create a generator that represents reading from a stream.
 * 
 * @param protocol: The protocol. You probably want file.
 * @param path: The path.
 * @param options: Options. You probably want the empty string.
 * 
 * This will be documented better when there's a manual and more than one type of protocol.
 * 
 * This is a shortcut for creating the stream yourself and adding it to a DecodingGenerator; advanced use cases can use the alternate path for other optimizations in future.
 * 
 * Note to maintainers: lives in src/generators/decoding.cpp, because it's a shortcut for that.
 * */
SYZ_CAPI syz_ErrorCode syz_createStreamingGenerator(syz_Handle *out, syz_Handle context, const char *protocol, const char *path, const char *options);

/*
 * A Buffer is decoded audio data.
 * 
 * This creates one from the 3 streaming parameters.
 * */
SYZ_CAPI syz_ErrorCode syz_createBufferFromStream(syz_Handle *out, const char *protocol, const char *path, const char *options);

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

struct syz_RouteConfig {
	float gain;
	float fade_time;
};

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
