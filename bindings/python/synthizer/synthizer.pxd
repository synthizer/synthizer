cdef extern from "synthizer.h":

    ctypedef unsigned long long syz_Handle

    ctypedef int syz_ErrorCode

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

    syz_ErrorCode syz_handleFree(syz_Handle handle)

    syz_ErrorCode syz_initialize()

    syz_ErrorCode syz_shutdown()

    syz_ErrorCode syz_getI(int* out, syz_Handle target, int property)

    syz_ErrorCode syz_setI(syz_Handle target, int property, int value)

    syz_ErrorCode syz_getD(double* out, syz_Handle target, int property)

    syz_ErrorCode syz_setD(syz_Handle target, int property, double value)

    syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value)

    syz_ErrorCode syz_getD3(double* x, double* y, double* z, syz_Handle target, int property)

    syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z)

    syz_ErrorCode syz_getD6(double* x1, double* y1, double* z1, double* x2, double* y2, double* z2, syz_Handle target, int property)

    syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2)

    syz_ErrorCode syz_createContext(syz_Handle* out)

    syz_ErrorCode syz_createContextHeadless(syz_Handle* out)

    syz_ErrorCode syz_contextGetBlock(syz_Handle context, float* block)

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
