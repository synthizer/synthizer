cdef extern from "synthizer.h":

    ctypedef unsigned long long syz_Handle

    ctypedef int syz_ErrorCode

    cdef struct syz_Event:
        int type
        syz_Handle source
        syz_Handle context

    void syz_eventDeinit(syz_Event* event)

    cdef enum SYZ_LOGGING_BACKEND:
        SYZ_LOGGING_BACKEND_NONE
        SYZ_LOGGING_BACKEND_STDERR

    cdef enum SYZ_LOG_LEVEL:
        SYZ_LOG_LEVEL_ERROR
        SYZ_LOG_LEVEL_WARN
        SYZ_LOG_LEVEL_INFO
        SYZ_LOG_LEVEL_DEBUG

    cdef struct syz_LibraryConfig:
        unsigned int log_level
        unsigned int logging_backend
        char* libsndfile_path

    void syz_libraryConfigSetDefaults(syz_LibraryConfig* config)

    syz_ErrorCode syz_initialize()

    syz_ErrorCode syz_initializeWithConfig(syz_LibraryConfig* config)

    syz_ErrorCode syz_shutdown() nogil

    syz_ErrorCode syz_getLastErrorCode()

    char* syz_getLastErrorMessage()

    syz_ErrorCode syz_handleIncRef(syz_Handle handle)

    syz_ErrorCode syz_handleDecRef(syz_Handle handle)

    cdef struct syz_DeleteBehaviorConfig:
        int linger
        double linger_timeout

    void syz_initDeleteBehaviorConfig(syz_DeleteBehaviorConfig* cfg)

    syz_ErrorCode syz_configDeleteBehavior(syz_Handle object, syz_DeleteBehaviorConfig* cfg)

    syz_ErrorCode syz_handleGetObjectType(int* out, syz_Handle handle)

    syz_ErrorCode syz_handleGetUserdata(void** out, syz_Handle handle)

    ctypedef void syz_UserdataFreeCallback(void*)

    syz_ErrorCode syz_handleSetUserdata(syz_Handle handle, void* userdata, syz_UserdataFreeCallback* free_callback)

    syz_ErrorCode syz_pause(syz_Handle object)

    syz_ErrorCode syz_play(syz_Handle object)

    syz_ErrorCode syz_getI(int* out, syz_Handle target, int property)

    syz_ErrorCode syz_setI(syz_Handle target, int property, int value)

    syz_ErrorCode syz_getD(double* out, syz_Handle target, int property)

    syz_ErrorCode syz_setD(syz_Handle target, int property, double value)

    syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value)

    syz_ErrorCode syz_getD3(double* x, double* y, double* z, syz_Handle target, int property)

    syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z)

    syz_ErrorCode syz_getD6(double* x1, double* y1, double* z1, double* x2, double* y2, double* z2, syz_Handle target, int property)

    syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2)

    cdef struct syz_BiquadConfig:
        double _b0
        double _b1
        double _b2
        double _a1
        double _a2
        double _gain
        unsigned char _is_wire

    syz_ErrorCode syz_getBiquad(syz_BiquadConfig* filter, syz_Handle target, int property)

    syz_ErrorCode syz_setBiquad(syz_Handle target, int property, syz_BiquadConfig* filter)

    syz_ErrorCode syz_biquadDesignIdentity(syz_BiquadConfig* filter)

    syz_ErrorCode syz_biquadDesignLowpass(syz_BiquadConfig* filter, double frequency, double q)

    syz_ErrorCode syz_biquadDesignHighpass(syz_BiquadConfig* filter, double frequency, double q)

    syz_ErrorCode syz_biquadDesignBandpass(syz_BiquadConfig* filter, double frequency, double bw)

    cdef struct syz_AutomationPoint:
        unsigned int interpolation_type
        double automation_time
        double values[6]
        unsigned long long flags

    syz_ErrorCode syz_createAutomationTimeline(syz_Handle* out, unsigned int point_count, syz_AutomationPoint* points, unsigned long long flags, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_automationSetTimeline(syz_Handle object, int property, syz_Handle timeline)

    syz_ErrorCode syz_automationClear(syz_Handle objeect, int property)

    syz_ErrorCode syz_createContext(syz_Handle* out, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createContextHeadless(syz_Handle* out, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_contextGetBlock(syz_Handle context, float* block)

    syz_ErrorCode syz_contextEnableEvents(syz_Handle context)

    syz_ErrorCode syz_contextGetNextEvent(syz_Event* out, syz_Handle context, unsigned long long flags)

    ctypedef int syz_StreamReadCallback(unsigned long long* read, unsigned long long requested, char* destination, void* userdata, char** err_msg)

    ctypedef int syz_StreamSeekCallback(unsigned long long pos, void* userdata, char** err_msg)

    ctypedef int syz_StreamCloseCallback(void* userdata, char** err_msg)

    ctypedef void syz_StreamDestroyCallback(void* userdata)

    cdef struct syz_CustomStreamDef:
        syz_StreamReadCallback* read_cb
        syz_StreamSeekCallback* seek_cb
        syz_StreamCloseCallback* close_cb
        syz_StreamDestroyCallback* destroy_cb
        long long length
        void* userdata

    ctypedef int syz_StreamOpenCallback(syz_CustomStreamDef* callbacks, char* protocol, char* path, void* param, void* userdata, char** err_msg)

    syz_ErrorCode syz_registerStreamProtocol(char* protocol, syz_StreamOpenCallback* callback, void* userdata)

    syz_ErrorCode syz_createStreamHandleFromStreamParams(syz_Handle* out, char* protocol, char* path, void* param, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamHandleFromMemory(syz_Handle* out, unsigned long long data_len, char* data, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamHandleFromFile(syz_Handle* out, char* path, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamHandleFromCustomStream(syz_Handle* out, syz_CustomStreamDef* callbacks, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamingGeneratorFromStreamParams(syz_Handle* out, syz_Handle context, char* protocol, char* path, void* param, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamingGeneratorFromFile(syz_Handle* out, syz_Handle context, char* path, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createStreamingGeneratorFromStreamHandle(syz_Handle* out, syz_Handle context, syz_Handle stream, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createBufferFromStreamParams(syz_Handle* out, char* protocol, char* path, void* param, void* userdata, syz_UserdataFreeCallback* userdata_free_callback) nogil

    syz_ErrorCode syz_createBufferFromEncodedData(syz_Handle* out, unsigned long long data_len, char* data, void* userdata, syz_UserdataFreeCallback* userdata_free_callback) nogil

    syz_ErrorCode syz_createBufferFromFloatArray(syz_Handle* out, unsigned int sr, unsigned int channels, unsigned long long frames, float* data, void* userdata, syz_UserdataFreeCallback* userdata_free_callback) nogil

    syz_ErrorCode syz_createBufferFromFile(syz_Handle* out, char* path, void* userdata, syz_UserdataFreeCallback* userdata_free_callback) nogil

    syz_ErrorCode syz_createBufferFromStreamHandle(syz_Handle* out, syz_Handle stream, void* userdata, syz_UserdataFreeCallback* userdata_free_callback) nogil

    syz_ErrorCode syz_bufferGetChannels(unsigned int* out, syz_Handle buffer)

    syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int* out, syz_Handle buffer)

    syz_ErrorCode syz_bufferGetLengthInSeconds(double* out, syz_Handle buffer)

    syz_ErrorCode syz_createBufferGenerator(syz_Handle* out, syz_Handle context, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator)

    syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator)

    syz_ErrorCode syz_createDirectSource(syz_Handle* out, syz_Handle context, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createAngularPannedSource(syz_Handle* out, syz_Handle context, int panner_strategy, double azimuth, double elevation, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createScalarPannedSource(syz_Handle* out, syz_Handle context, int panner_strategy, double scalar, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createSource3D(syz_Handle* out, syz_Handle context, int panner_strategy, double x, double y, double z, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    syz_ErrorCode syz_createNoiseGenerator(syz_Handle* out, syz_Handle context, unsigned int channels, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    cdef struct syz_RouteConfig:
        double gain
        double fade_time
        syz_BiquadConfig filter

    syz_ErrorCode syz_initRouteConfig(syz_RouteConfig* cfg)

    syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, syz_RouteConfig* config)

    syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, double fade_out)

    syz_ErrorCode syz_effectReset(syz_Handle effect)

    syz_ErrorCode syz_createGlobalEcho(syz_Handle* out, syz_Handle context, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)

    cdef struct syz_EchoTapConfig:
        double delay
        double gain_l
        double gain_r

    syz_ErrorCode syz_globalEchoSetTaps(syz_Handle handle, unsigned int n_taps, syz_EchoTapConfig* taps)

    syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle* out, syz_Handle context, void* userdata, syz_UserdataFreeCallback* userdata_free_callback)
