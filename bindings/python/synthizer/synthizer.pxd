cdef extern from "synthizer.h":

    ctypedef unsigned long long syz_Handle

    ctypedef int syz_ErrorCode

    cdef struct syz_EventFinished:
        char unused

    cdef struct syz_EventLooped:
        unsigned long long loop_counter

    cdef union _syz_Event_payload_u:
        syz_EventLooped looped
        syz_EventFinished finished

    cdef struct syz_Event:
        int type
        syz_Handle source
        syz_Handle context
        void* userdata
        _syz_Event_payload_u payload

    cdef enum SYZ_LOGGING_BACKEND:
        SYZ_LOGGING_BACKEND_STDERR

    syz_ErrorCode syz_configureLoggingBackend(SYZ_LOGGING_BACKEND backend, void* param)

    cdef enum SYZ_LOG_LEVEL:
        SYZ_LOG_LEVEL_ERROR
        SYZ_LOG_LEVEL_WARN
        SYZ_LOG_LEVEL_INFO
        SYZ_LOG_LEVEL_DEBUG

    void syz_setLogLevel(SYZ_LOG_LEVEL level)

    syz_ErrorCode syz_getLastErrorCode()

    char* syz_getLastErrorMessage()

    syz_ErrorCode syz_initialize()

    syz_ErrorCode syz_shutdown() nogil

    syz_ErrorCode syz_handleFree(syz_Handle handle)

    syz_ErrorCode syz_handleGetObjectType(int* out, syz_Handle handle)

    syz_ErrorCode syz_getUserdata(void** out, syz_Handle handle)

    ctypedef void syz_UserdataFreeCallback(void*)

    syz_ErrorCode syz_setUserdata(syz_Handle handle, void* userdata, syz_UserdataFreeCallback* free_callback)

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
        double b0
        double b1
        double b2
        double a1
        double a2
        double gain
        unsigned char is_wire

    syz_ErrorCode syz_getBiquad(syz_BiquadConfig* filter, syz_Handle target, int property)

    syz_ErrorCode syz_setBiquad(syz_Handle target, int property, syz_BiquadConfig* filter)

    syz_ErrorCode syz_biquadDesignIdentity(syz_BiquadConfig* filter)

    syz_ErrorCode syz_biquadDesignLowpass(syz_BiquadConfig* filter, double frequency, double q)

    syz_ErrorCode syz_biquadDesignHighpass(syz_BiquadConfig* filter, double frequency, double q)

    syz_ErrorCode syz_biquadDesignBandpass(syz_BiquadConfig* filter, double frequency, double bw)

    syz_ErrorCode syz_createContext(syz_Handle* out)

    syz_ErrorCode syz_createContextHeadless(syz_Handle* out)

    syz_ErrorCode syz_contextGetBlock(syz_Handle context, float* block)

    syz_ErrorCode syz_contextEnableEvents(syz_Handle context)

    syz_ErrorCode syz_contextGetNextEvent(syz_Event* out, syz_Handle context)

    syz_ErrorCode syz_createStreamingGenerator(syz_Handle* out, syz_Handle context, char* protocol, char* path, char* options)

    syz_ErrorCode syz_createBufferFromStream(syz_Handle* out, char* protocol, char* path, char* options) nogil

    syz_ErrorCode syz_bufferGetChannels(unsigned int* out, syz_Handle buffer)

    syz_ErrorCode syz_bufferGetLengthInSamples(unsigned int* out, syz_Handle buffer)

    syz_ErrorCode syz_bufferGetLengthInSeconds(double* out, syz_Handle buffer)

    syz_ErrorCode syz_createBufferGenerator(syz_Handle* out, syz_Handle context)

    syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator)

    syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator)

    syz_ErrorCode syz_createDirectSource(syz_Handle* out, syz_Handle context)

    syz_ErrorCode syz_createPannedSource(syz_Handle* out, syz_Handle context)

    syz_ErrorCode syz_createSource3D(syz_Handle* out, syz_Handle context)

    syz_ErrorCode syz_createNoiseGenerator(syz_Handle* out, syz_Handle context, unsigned int channels)

    cdef struct syz_RouteConfig:
        float gain
        float fade_time
        syz_BiquadConfig filter

    syz_ErrorCode syz_initRouteConfig(syz_RouteConfig* cfg)

    syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, syz_RouteConfig* config)

    syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, float fade_out)

    syz_ErrorCode syz_effectReset(syz_Handle effect)

    syz_ErrorCode syz_createGlobalEcho(syz_Handle* out, syz_Handle context)

    cdef struct syz_EchoTapConfig:
        float delay
        float gain_l
        float gain_r

    syz_ErrorCode syz_echoSetTaps(syz_Handle handle, unsigned int n_taps, syz_EchoTapConfig* taps)

    syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle* out, syz_Handle context)
