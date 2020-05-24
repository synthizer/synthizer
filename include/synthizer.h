#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/* Inject DLL attributes etc here. */
#ifdef _WIN32
	#ifdef BUILDING_SYNTHIZER
		#define SYZ_CAPI __declspec(dllexport)
	#else
		#define SYZ_CAPI
	#endif
#else
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
typedef unsigned int syz_Handle;

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
 * Handle reference counting functions.  Note that:
 * - Handles from a create function always have a refcount of 1.
 * - Handles to a callback only live as long as the callback unless you had a reference previously.
 * */
SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle);

/*
 * Library initialization and shutdown.
 * 
 * This must be called at least once per program wishing to use synthizer. For convenience when dealing with weird binding situations, these nest; call syz_shutdown as many times as you call syz_initialize.
 * */
SYZ_CAPI syz_ErrorCode syz_initialize();
SYZ_CAPI syz_ErrorCode syz_shutdown();

/*
 * Property getters and setters.
 * 
 * See synthizer_constants.h and synthizer_properties.h for the enums and property definitions respectively.
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
SYZ_CAPI syz_ErrorCode syz_getO(syz_Handle *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);

/*
 * Create a context. This represents the audio device itself, and all other Synthizer objects need one.
 * */
SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out);

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
 * Add/remove generators from a PannedSource. The PannedSource weak references the generators; they must be kept alive on the external side for the time being.
 * 
 * This will probably change as lifetimes get narrowed down, and as sources are extended with more functionality to manage their generators.
 * 
 * Each generator may only be added to a source once. Duplicate calls are ignored.
 * If a generator isn't on a source, the call silently does nothing.
 * */
SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);

/*
 * Create a panned source, a source with azimuth/elevation as the underlying panning strategy.
 * 
 * For spatialized audio like games, use SpatializedSource, which has x/y/z and other interesting spatialization properties.
 * */
SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context);

#ifdef __cplusplus
}
#endif
