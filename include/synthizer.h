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
 * Note to maintainers: C API methods that obviously go with  a type live in the main .cpp file for that type, even if
 * it is necessary to introduce a cpp file for that type to hold the C API (i.e. pure abstract classes).
 *
 * Other methods are in src/c_api.cpp (i.e. generic property setters, initialization, etc.).
 * */

SYZ_CAPI void syz_getVersion(unsigned int *major, unsigned int *minor, unsigned int *patch);

typedef unsigned long long syz_Handle;
typedef int syz_ErrorCode;

struct syz_UserAutomationEvent {
  unsigned long long param;
};

struct syz_Event {
  int type;
  syz_Handle source;
  syz_Handle context;
  union {
    struct syz_UserAutomationEvent user_automation;
  } payload;
};

SYZ_CAPI void syz_eventDeinit(struct syz_Event *event);

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
SYZ_CAPI syz_ErrorCode syz_initializeWithConfig(const struct syz_LibraryConfig *config);
SYZ_CAPI syz_ErrorCode syz_shutdown();

SYZ_CAPI syz_ErrorCode syz_getLastErrorCode(void);
SYZ_CAPI const char *syz_getLastErrorMessage(void);

SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle);

struct syz_DeleteBehaviorConfig {
  int linger;
  double linger_timeout;
};

SYZ_CAPI void syz_initDeleteBehaviorConfig(struct syz_DeleteBehaviorConfig *cfg);
SYZ_CAPI syz_ErrorCode syz_configDeleteBehavior(syz_Handle object, const struct syz_DeleteBehaviorConfig *cfg);

SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle);

SYZ_CAPI syz_ErrorCode syz_handleGetUserdata(void **out, syz_Handle handle);
typedef void syz_UserdataFreeCallback(void *);
SYZ_CAPI syz_ErrorCode syz_handleSetUserdata(syz_Handle handle, void *userdata,
                                             syz_UserdataFreeCallback *free_callback);

SYZ_CAPI syz_ErrorCode syz_pause(syz_Handle object);
SYZ_CAPI syz_ErrorCode syz_play(syz_Handle object);

SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value);
SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value);
SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);
SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z);
SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2,
                                 syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2,
                                 double z2);

struct syz_BiquadConfig {
  double _b0, _b1, _b2;
  double _a1, _a2;
  double _gain;
  unsigned char _is_wire;
};

SYZ_CAPI syz_ErrorCode syz_getBiquad(struct syz_BiquadConfig *filter, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setBiquad(syz_Handle target, int property, const struct syz_BiquadConfig *filter);

SYZ_CAPI syz_ErrorCode syz_biquadDesignIdentity(struct syz_BiquadConfig *filter);
SYZ_CAPI syz_ErrorCode syz_biquadDesignLowpass(struct syz_BiquadConfig *filter, double frequency, double q);
SYZ_CAPI syz_ErrorCode syz_biquadDesignHighpass(struct syz_BiquadConfig *filter, double frequency, double q);
SYZ_CAPI syz_ErrorCode syz_biquadDesignBandpass(struct syz_BiquadConfig *filter, double frequency, double bw);

struct syz_AutomationPoint {
  unsigned int interpolation_type;
  double values[6];
  unsigned long long flags;
};

struct syz_AutomationAppendPropertyCommand {
  int property;
  struct syz_AutomationPoint point;
};

struct syz_AutomationClearPropertyCommand {
  int property;
};

struct syz_AutomationSendUserEventCommand {
  unsigned long long param;
};

struct syz_AutomationCommand {
  syz_Handle target;
  double time;
  int type;
  unsigned int flags;
  union {
    struct syz_AutomationAppendPropertyCommand append_to_property;
    struct syz_AutomationClearPropertyCommand clear_property;
    struct syz_AutomationSendUserEventCommand send_user_event;
  } params;
};

SYZ_CAPI syz_ErrorCode syz_createAutomationBatch(syz_Handle *out, syz_Handle context, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_automationBatchAddCommands(syz_Handle batch, unsigned long long commands_len,
                                                      const struct syz_AutomationCommand *commands);
SYZ_CAPI syz_ErrorCode syz_automationBatchExecute(syz_Handle batch);

SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out, void *userdata,
                                         syz_UserdataFreeCallback *userdata_free_callback);

/*
 * Create a context which doesn't feed an audio device.
 * Currently this is exposed for testing purposes, but will eventually become a stable and useful API.
 * */
SYZ_CAPI syz_ErrorCode syz_createContextHeadless(syz_Handle *out, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback);
/*
 * get a block of audio with 2 channels. This is also
 * currently only exposed for testing, especially since "block" is not part of the external API and using this wrong is
 * a good way to crash for the time being.
 * */
SYZ_CAPI syz_ErrorCode syz_contextGetBlock(syz_Handle context, float *block);

SYZ_CAPI syz_ErrorCode syz_contextEnableEvents(syz_Handle context);
SYZ_CAPI syz_ErrorCode syz_contextGetNextEvent(struct syz_Event *out, syz_Handle context, unsigned long long flags);

typedef int syz_StreamReadCallback(unsigned long long *read, unsigned long long requested, char *destination,
                                   void *userdata, const char **err_msg);
typedef int syz_StreamSeekCallback(unsigned long long pos, void *userdata, const char **err_msg);
typedef int syz_StreamCloseCallback(void *userdata, const char **err_msg);
typedef void syz_StreamDestroyCallback(void *userdata);

struct syz_CustomStreamDef {
  syz_StreamReadCallback *read_cb;
  syz_StreamSeekCallback *seek_cb;
  syz_StreamCloseCallback *close_cb;
  syz_StreamDestroyCallback *destroy_cb;
  long long length;
  void *userdata;
};

typedef int syz_StreamOpenCallback(struct syz_CustomStreamDef *callbacks, const char *protocol, const char *path,
                                   void *param, void *userdata, const char **err_msg);
SYZ_CAPI syz_ErrorCode syz_registerStreamProtocol(const char *protocol, syz_StreamOpenCallback *callback,
                                                  void *userdata);

SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromStreamParams(syz_Handle *out, const char *protocol, const char *path,
                                                              void *param, void *userdata,
                                                              syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromMemory(syz_Handle *out, unsigned long long data_len, const char *data,
                                                        void *userdata,
                                                        syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromFile(syz_Handle *out, const char *path, void *userdata,
                                                      syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromCustomStream(syz_Handle *out, struct syz_CustomStreamDef *callbacks,
                                                              void *userdata,
                                                              syz_UserdataFreeCallback *userdata_free_callback);

SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamParams(syz_Handle *out, syz_Handle context,
                                                                    const char *protocol, const char *path, void *param,
                                                                    void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromFile(syz_Handle *out, syz_Handle context, const char *path,
                                                            void *config, void *userdata,
                                                            syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createStreamingGeneratorFromStreamHandle(syz_Handle *out, syz_Handle context,
                                                                    syz_Handle stream, void *config, void *userdata,
                                                                    syz_UserdataFreeCallback *userdata_free_callback);

SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamParams(syz_Handle *out, const char *protocol, const char *path,
                                                        void *param, void *userdata,
                                                        syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createBufferFromEncodedData(syz_Handle *out, unsigned long long data_len, const char *data,
                                                       void *userdata,
                                                       syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createBufferFromFloatArray(syz_Handle *out, unsigned int sr, unsigned int channels,
                                                      unsigned long long frames, const float *data, void *userdata,
                                                      syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createBufferFromFile(syz_Handle *out, const char *path, void *userdata,
                                                syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createBufferFromStreamHandle(syz_Handle *out, syz_Handle stream, void *userdata,
                                                        syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_bufferGetChannels(unsigned int *out, syz_Handle buffer);
SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int *out, syz_Handle buffer);
SYZ_CAPI syz_ErrorCode syz_bufferGetLengthInSeconds(double *out, syz_Handle buffer);

SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback);

SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);

SYZ_CAPI syz_ErrorCode syz_createDirectSource(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                              syz_UserdataFreeCallback *userdata_free_callback);

SYZ_CAPI syz_ErrorCode syz_createAngularPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                     double azimuth, double elevation, void *config, void *userdata,
                                                     syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createScalarPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                    double panning_scalar, void *config, void *userdata,
                                                    syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createSource3D(syz_Handle *out, syz_Handle context, int panner_strategy, double x, double y,
                                          double z, void *config, void *userdata,
                                          syz_UserdataFreeCallback *userdata_free_callback);

SYZ_CAPI syz_ErrorCode syz_createNoiseGenerator(syz_Handle *out, syz_Handle context, unsigned int channels,
                                                void *config, void *userdata,
                                                syz_UserdataFreeCallback *userdata_free_callback);

struct syz_RouteConfig {
  double gain;
  double fade_time;
  struct syz_BiquadConfig filter;
};

SYZ_CAPI syz_ErrorCode syz_initRouteConfig(struct syz_RouteConfig *cfg);

SYZ_CAPI syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input,
                                              const struct syz_RouteConfig *config);
SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, double fade_out);

SYZ_CAPI syz_ErrorCode syz_effectReset(syz_Handle effect);

SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                            syz_UserdataFreeCallback *userdata_free_callback);

struct syz_EchoTapConfig {
  double delay;
  double gain_l;
  double gain_r;
};

SYZ_CAPI syz_ErrorCode syz_globalEchoSetTaps(syz_Handle handle, unsigned int n_taps,
                                             const struct syz_EchoTapConfig *taps);

SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                                 syz_UserdataFreeCallback *userdata_free_callback);

#ifdef __cplusplus
}
#endif
