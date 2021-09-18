#cython: auto_pickle=False
# distutils: language = c++
import contextlib
import threading
from enum import Enum
import sys

from synthizer_constants cimport *


from cpython.mem cimport PyMem_Malloc, PyMem_Realloc, PyMem_Free
from cpython.ref cimport PyObject, Py_DECREF, Py_INCREF

# We want the ability to acquire and release the GIL, which means making sure it's initialized.
# It's unclear if you need this for with nogil as well as for with gil, but let's just avoid the headache entirely.
cdef extern from "Python.h":
    int PyEval_InitThreads()
PyEval_InitThreads()
cdef extern from "string.h":
    void *memcpy(void *dest, const void *src, size_t count)

# We need memcpy for custom streams.

# An internal sentinel for an unset default parameter. Used because
# None is sometimes used.
class _DefaultSentinel:
    pass

DEFAULT_VALUE = _DefaultSentinel()

cdef class SynthizerError(Exception):
    cdef str message
    cdef int code

    def __init__(self):
        self.code = syz_getLastErrorCode()
        cdef const char *tmp = syz_getLastErrorMessage()
        if tmp == NULL:
            self.message = ""
        else:
            self.message = str(tmp, "utf-8")

    def __str__(self):
        return f"SynthizerError: {self.message} [{self.code}]"

cdef _checked(x):
    if x != 0:
        raise SynthizerError()

cdef bytes _to_bytes(x):
    if isinstance(x, bytes):
        return x
    return bytes(x, "utf-8")

# These are descriptors for properties so that we don't have to continually deal with defining 6 line method pairs.
# These might eventually prove to be too slow, in which case we will have to convert to get_xxx and set_xxx methods.
# Unfortunately Cython doesn't give us a convenient way to generate such at compile time.
# Fortunately there's a way to migrate people forward if we must.
cdef class _PropertyBase:
    def __delete__(self, instance):
        raise RuntimeError("Deleting attributes from Synthizer objects is not possible")

cdef class IntProperty(_PropertyBase):
    cdef int property
    cdef object conv_in
    cdef object conv_out


    def __init__(self, int property, conv_in = lambda x: x, conv_out = lambda x: x):
        self.property = property
        self.conv_in = conv_in
        self.conv_out = conv_out

    def __get__(self, _BaseObject instance, owner):
        cdef int val
        _checked(syz_getI(&val, instance.handle, self.property))
        return self.conv_out(val)

    def __set__(self, _BaseObject instance, value):
        _checked(syz_setI(instance.handle, self.property, self.conv_in(value)))

def enum_property(v, e):
    return IntProperty(v, conv_in = lambda x: x.value, conv_out = lambda x: e(x))

cdef class DoubleProperty(_PropertyBase):
    cdef int property

    def __init__(self, int property):
        self.property = property

    def __get__(self, _BaseObject instance, owner):
        cdef double val
        _checked(syz_getD(&val, instance.handle, self.property))
        return val

    def __set__(self, _BaseObject instance, double value):
        _checked(syz_setD(instance.handle, self.property, value))

cdef class Double3Property(_PropertyBase):
    cdef int property

    def __init__(self, int property):
        self.property = property

    def __get__(self, _BaseObject instance, owner):
        cdef double x, y, z
        _checked(syz_getD3(&x, &y, &z, instance.handle, self.property))
        return (x, y, z)

    def __set__(self, _BaseObject instance, value):
        _checked(syz_setD3(instance.handle, self.property, value[0], value[1], value[2]))

cdef class Double6Property(_PropertyBase):
    cdef int property

    def __init__(self, int property):
        self.property = property

    def __get__(self, _BaseObject instance, owner):
        cdef double x1, y1, z1, x2, y2, z2
        _checked(syz_getD6(&x1, &y1, &z1, &x2, &y2, &z2, instance.handle, self.property))
        return (x1, y1, z1, x2, y2, z2)

    def __set__(self, _BaseObject instance, value):
        _checked(syz_setD6(instance.handle, self.property, value[0], value[1], value[2], value[3], value[4], value[5]))

cdef class BiquadProperty(_PropertyBase):
    cdef int property

    def __init__(self, int property):
        self.property = property

    def __get__(self, _BaseObject instance, owner):
            raise NotImplementedError("Filter properties cannot be read")

    def __set__(self, _BaseObject instance, BiquadConfig value):
        _checked(syz_setBiquad(instance.handle, self.property, &value.config))

cdef class ObjectProperty(_PropertyBase):
    cdef int property
    cdef object cls

    def __init__(self, int property, cls):
        self.property = property
        self.cls = cls

    def __get__(self, _BaseObject instance, owner):
            raise NotImplementedError("Can't read object properties")
    def __set__(self, _BaseObject instance, _BaseObject value):
        _checked(syz_setO(instance.handle, self.property, value.handle if value else 0))

class LogLevel(Enum):
    ERROR = SYZ_LOG_LEVEL_ERROR
    WARN = SYZ_LOG_LEVEL_WARN
    INFO = SYZ_LOG_LEVEL_INFO
    DEBUG = SYZ_LOG_LEVEL_DEBUG

class LoggingBackend(Enum):
    NONE = SYZ_LOGGING_BACKEND_NONE
    STDERR = SYZ_LOGGING_BACKEND_STDERR

cpdef initialize(log_level=DEFAULT_VALUE, logging_backend=DEFAULT_VALUE,
    libsndfile_path=DEFAULT_VALUE):
    """Initialize Synthizer.  Try synthizer.Initialized for a context manager that will shut things down on exceptions. """
    cdef syz_LibraryConfig cfg
    syz_libraryConfigSetDefaults(&cfg)
    if log_level is not DEFAULT_VALUE:
        cfg.log_level = log_level.value
    if logging_backend is not DEFAULT_VALUE:
        cfg.logging_backend = logging_backend.value
    if libsndfile_path is not DEFAULT_VALUE:
        libsndfile_path = _to_bytes(libsndfile_path)
        cfg.libsndfile_path = libsndfile_path
    _checked(syz_initializeWithConfig(&cfg))

cpdef shutdown():
    """Shut Synthizer down."""
    cdef syz_ErrorCode ecode
    # If this isn't nogil, then userdata in the Synthizer background thread deadlocks because
    # syz_shutdown waits for that queue to drain but this is holding the lock.
    with nogil:
        ecode = syz_shutdown()
    _checked(ecode)

@contextlib.contextmanager
def initialized(*args, **kwargs):
    """A context manager that safely initializes and shuts Synthizer down.

    Usage:

    with synthizer.initialized():
        ...my code

    All arguments are passed to initialize
    """
    initialize(*args, **kwargs)
    try:
        yield
    except BaseException as e:
        raise e
    finally:
        shutdown()

class PannerStrategy(Enum):
    DELEGATE = SYZ_PANNER_STRATEGY_DELEGATE
    HRTF = SYZ_PANNER_STRATEGY_HRTF
    STEREO = SYZ_PANNER_STRATEGY_STEREO

class DistanceModel(Enum):
    NONE = SYZ_DISTANCE_MODEL_NONE
    LINEAR = SYZ_DISTANCE_MODEL_LINEAR
    EXPONENTIAL = SYZ_DISTANCE_MODEL_EXPONENTIAL
    INVERSE = SYZ_DISTANCE_MODEL_INVERSE

cdef object _objects_by_handle = dict()
cdef object _objects_by_handle_mutex = threading.Lock()

cdef _register_object(_BaseObject obj):
    with _objects_by_handle_mutex:
        _objects_by_handle[obj.handle] = obj

cdef _unregister_object(_BaseObject obj):
    with _objects_by_handle_mutex:
        del _objects_by_handle[obj.handle]

cdef _handle_to_object(handle):
    with _objects_by_handle_mutex:
        return _objects_by_handle.get(handle, None)

cdef void userdataFree(void *userdata) with gil:
    Py_DECREF(<object>userdata)

cdef class _UserdataBox:
    """An internal box for containing userdata.  This exists so that we can have
    multiple userdata values associated with an object, which we use to keep
    buffers alive for stream handles."""
    cdef object userdata
    # Used by StreamHandle
    cdef object stream_buffer

    def __init__(self):
        self.userdata = None
        self.stream_buffer = None

cdef class _BaseObject:
    cdef syz_Handle handle

    def __init__(self, syz_Handle handle):
        self.handle = handle
        _register_object(self)
        ubox = _UserdataBox()
        cdef PyObject *ud = <PyObject *>ubox
        Py_INCREF(ubox)
        _checked(syz_handleSetUserdata(self.handle, <void *>ud, userdataFree))

    def dec_ref(self):
        """Decrement the reference count. Must be called in order to not leak Synthizer objects."""
        _checked(syz_handleDecRef(self.handle))
        _unregister_object(self)
        self.handle = 0

    cpdef syz_Handle _get_handle_checked(self, cls):
        #Internal: get a handle, after checking that this object is a subclass of the specified class.
        if not isinstance(self, cls):
            raise ValueError("Synthizer object is of an unexpected type")
        return self.handle

    cdef object _get_userdata_box(self):
        cdef void *userdata
        _checked(syz_handleGetUserdata(&userdata, self.handle))
        return <_UserdataBox>userdata

    cpdef get_userdata(self):
        cdef _UserdataBox box = self._get_userdata_box()
        return box.userdata

    cpdef set_userdata(self, userdata):
        cdef _UserdataBox box = self._get_userdata_box()
        box.userdata = userdata

    def config_delete_behavior(self, linger = _DefaultSentinel, linger_timeout = _DefaultSentinel):
        cdef syz_DeleteBehaviorConfig cfg
        syz_initDeleteBehaviorConfig(&cfg)
        if linger is not _DefaultSentinel:
            cfg.linger = linger
        if linger_timeout is not _DefaultSentinel:
            cfg.linger_timeout = linger_timeout
        _checked(syz_configDeleteBehavior(self.handle, &cfg))

cdef class Pausable(_BaseObject):
    """Base class for anything which can be paused. Adds pause and play methods."""

    def play(self):
        _checked(syz_play(self.handle))

    def pause(self):
        _checked(syz_pause(self.handle))

cdef class Event:
    """Base class for all Synthizer events"""
    cpdef public Context context
    cpdef public object source

    def __init__(self, context, source):
        self.context = context
        self.source = source


cdef class FinishedEvent(Event):
    pass

cdef class LoopedEvent(Event):
    pass

cdef _convert_event(syz_Event event):
    if event.type == SYZ_EVENT_TYPE_FINISHED:
        return FinishedEvent(_handle_to_object(event.context), _handle_to_object(event.source))
    elif event.type == SYZ_EVENT_TYPE_LOOPED:
        return LoopedEvent(_handle_to_object(event.context), _handle_to_object(event.source))

cdef class BiquadConfig:
    cdef syz_BiquadConfig config

    def __init__(self):
        """Initializes the BiquadConfig as an identity filter.
        Use one of the design_XXX staticmethods to get a useful filter."""
        _checked(syz_biquadDesignIdentity(&self.config))

    @staticmethod
    def design_identity():
        cdef BiquadConfig out = BiquadConfig()
        _checked(syz_biquadDesignIdentity(&out.config))
        return out

    @staticmethod
    def design_lowpass(frequency, q = 0.7071135624381276):
        cdef BiquadConfig out = BiquadConfig()
        _checked(syz_biquadDesignLowpass(&out.config, frequency, q))
        return out

    @staticmethod
    def design_highpass(frequency, q=0.7071135624381276):
        cdef BiquadConfig out = BiquadConfig()
        _checked(syz_biquadDesignHighpass(&out.config, frequency, q))
        return out

    @staticmethod
    def design_bandpass(frequency, bandwidth):
        cdef BiquadConfig out = BiquadConfig()
        _checked(syz_biquadDesignBandpass(&out.config, frequency, bandwidth))
        return out

cdef class Context(Pausable):
    """The Synthizer context represents an open audio device and groups all Synthizer objects created with it into one unit.

    To use Synthizer, the first step is to create a Context, which all other library types expect in their constructors.  When the context is destroyed, all audio playback stops and calls to
    methods on other objects in Synthizer will generally not have an effect.  Once the context is gone, the only operation that makes sense for other object types
    is to destroy them.  Put another way, keep the Context alive for the duration of your application."""

    def __init__(self, enable_events=False):
        cdef syz_Handle handle
        _checked(syz_createContext(&handle, NULL, NULL))
        super().__init__(handle)
        if enable_events:
            self.enable_events()

    gain = DoubleProperty(SYZ_P_GAIN)
    position = Double3Property(SYZ_P_POSITION)
    orientation = Double6Property(SYZ_P_ORIENTATION)
    default_distance_model = enum_property(SYZ_P_DEFAULT_DISTANCE_MODEL, lambda x: DistanceModel(x))
    default_distance_ref = DoubleProperty(SYZ_P_DEFAULT_DISTANCE_REF)
    default_distance_max = DoubleProperty(SYZ_P_DEFAULT_DISTANCE_MAX)
    default_rolloff = DoubleProperty(SYZ_P_DEFAULT_ROLLOFF)
    default_closeness_boost = DoubleProperty(SYZ_P_DEFAULT_CLOSENESS_BOOST)
    default_closeness_boost_distance = DoubleProperty(SYZ_P_DEFAULT_CLOSENESS_BOOST_DISTANCE)
    default_panner_strategy = enum_property(SYZ_P_DEFAULT_PANNER_STRATEGY, lambda x: PannerStrategy(x))

    cpdef config_route(self, _BaseObject output, _BaseObject input, gain = 1.0, fade_time = 0.01, BiquadConfig filter = None):
        cdef syz_RouteConfig config
        _checked(syz_initRouteConfig(&config))
        if filter:
            config.filter = filter.config
        config.gain = gain
        config.fade_time = fade_time
        _checked(syz_routingConfigRoute(self.handle, output.handle, input.handle, &config))

    cpdef remove_route(self, _BaseObject output, _BaseObject input, fade_time=0.01):
        _checked(syz_routingRemoveRoute(self.handle, output.handle, input.handle, fade_time))

    cpdef enable_events(self):
        _checked(syz_contextEnableEvents(self.handle))

    def get_events(self, limit=0):
        """Returns an iterator over events. Said iterator will
        drive Synthizer's syz_contextGetNextEvent interface, yielding the next event on every iteration. This allows for
        normal Python loops to consume events.

        Stopping iteration early is permitted: any events not drained from this iterator will show up in the next one.

        If limit is a non-zero value, at most limit events will be yielded. Otherwise, this iterator will continue
        until the queue is drained."""
        cdef syz_Event event
        cdef unsigned int drained_so_far
        if limit < 0:
            raise RuntimeError("Limit may not be negative")
        while True:
            _checked(syz_contextGetNextEvent(&event, self.handle, 0))
            if event.type == SYZ_EVENT_TYPE_INVALID:
                break
            will_yield = _convert_event(event)
            syz_eventDeinit(&event)
            yield will_yield
            drained_so_far += 1
            if limit != 0 and drained_so_far == limit:
                break

# Used to keep errors alive per the custom stream error lifetime rules in synthizer.h.
cdef class WrappedStream:
    cdef object stream
    cdef object last_err

    def __init__(self, stream):
        self.stream = stream
        self.last_err = None

cdef int custom_stream_read_cb(unsigned long long *wrote, unsigned long long requested, char *destination, void *userdata, const char **err_msg) with gil:
    cdef WrappedStream obj = <WrappedStream>userdata
    cdef stream = obj.stream
    cdef object read_data
    cdef const unsigned char[::1] out_view = None
    try:
        read_data = stream.read(requested)
        out_view = read_data
    except:
        (ign, e, ign2) = sys.exc_info()
        obj.last_err = _to_bytes(str(e))
        err_msg[0] = obj.last_err
        return 1
    wrote[0] = out_view.shape[0]
    if out_view.shape[0] != 0:
        memcpy(destination, &out_view[0], out_view.shape[0])
    return 0

cdef int custom_stream_seek_cb(unsigned long long pos, void *userdata, const char **err_msg) with gil:
    cdef WrappedStream obj = <WrappedStream>userdata
    cdef stream = obj.stream
    try:
        stream.seek(pos)
    except:
        (ign, e, ign2) = sys.exc_info()
        obj.last_err = _to_bytes(str(e))
        err_msg[0] = obj.last_err
        return 1
    return 0

cdef int custom_stream_close_cb(void *userdata, const char **err_msg) with gil:
    cdef WrappedStream obj = <WrappedStream>userdata
    cdef stream = obj.stream
    try:
        closer = getattr(stream, 'close')
        if callable(closer):
            closer()
    except:
        (ign, e, ign2) = sys.exc_info()
        obj.last_err = _to_bytes(str(e))
        err_msg[0] = obj.last_err
    return 0

cdef void custom_stream_destroy_cb(void *userdata) with gil:
    Py_DECREF(<object>userdata)

cdef custom_stream_fillout_callbacks(syz_CustomStreamDef *callbacks, object stream):
    cdef object length_getter = None
    cdef WrappedStream wrapped
    cdef unsigned long long length = -1
    if not callable(getattr(stream, 'read')):
        raise ValueError("Streams must have a read callback")
    length_getter = getattr(stream, 'get_length')
    if length_getter:
        if not callable(length_getter):
            raise ValueError("Stream has get_length, but it isn't callable")
        length = length_getter()
    callbacks.read_cb = &custom_stream_read_cb
    callbacks.length = length
    if callable(getattr(stream, 'seek')):
        callbacks.seek_cb = &custom_stream_seek_cb
    callbacks.close_cb = &custom_stream_close_cb
    wrapped = WrappedStream(stream)
    callbacks.userdata = <void *>wrapped
    callbacks.destroy_cb = custom_stream_destroy_cb
    Py_INCREF(<object>callbacks.userdata)

# Used to force the Python bindings to keep exceptions
# from opening custom streams around long enough to communicate the error to the caller.
cdef object last_custom_stream_open_error = threading.local()

cdef int custom_stream_open_cb(syz_CustomStreamDef *callbacks, const char *protocol, const char *path, void *param, void *userdata, const char **err_msg) with gil:
    cdef object obj = <object>userdata
    cdef object stream = None
    cdef object length_getter = None
    cdef WrappedStream wrapped
    cdef unsigned long long length = -1
    try:
        stream = obj(protocol.decode("utf-8"), path.decode("utf-8"), <unsigned long long>param)
        custom_stream_fillout_callbacks(callbacks, stream)
        return 0
    except:
        (ign, e, ign2) = sys.exc_info()
        last_custom_stream_open_error.err = _to_bytes(str(e))
        err_msg[0] = last_custom_stream_open_error.err
        Py_DECREF(stream)
        return 1

def register_stream_protocol(protocol, factory):
    """Register a custom stream protocol with the given name.  The factory
function must be callable as:

`factory(protocol: str, path: str, param: integer)`

And return an object with the following functions:

- `read(size)`: Must return a bytes object containing exactly size bytes.  Required.  if this function returns lss,
  it is considered end of stream.
- `seek(position)`: Optional. If present and if `get_length` is also present, this
  stream is sekable.  Must seek to `position` from the beginning of the stream.  Note that `position` may be one byte past the last readable byte, like with `file` objects.
- `get_length()`: Optional. Return the length of the stream in bytes. Must always return the same value.
- `close()`: Optional.  Close the stream.  Called when Synthizer is entirely done with the stream.  The object will not be used after this point, but may not be
  garbage collected immediately.

The stream object will be used by background threads and needs to be safe to send between threads.  It will only ever be used from one thread at a time.

The `factory` factory function/object will be leaked forever.  This should only be used
to set statically determined protocols.
"""
    cdef void *userdata = <void *>factory
    if not callable(factory):
        raise ValueError("factory must be a callable factory function")
    # We try registering first, then increment the reference.  Incrementing the reference
    # doesn't fail, but registering might.
    _checked(syz_registerStreamProtocol(_to_bytes(protocol), custom_stream_open_cb, userdata))
    # Now leak it.
    Py_INCREF(<object>userdata)

cdef class StreamHandle(_BaseObject):
    """Wraps the C API concept of a StreamHandle, which may be created in a variety of ways."""
    def __init__(self, _handle=None):
        if _handle is None:
            raise RuntimeError("Use one of the staticmethods to initialize Buffers in order to specify where the data comes from.")
        super().__init__(_handle)

    @staticmethod
    def from_stream_params(protocol, path, size_t param):
        """Create a StreamHandle from the 3 streaming parameters. The `param`
        parameter is a pointer in C, but exposed in Python as a size_t which is
        large enough to contain a pointer on all platforms.  If working with a
        custom stream that needs a value for `param`, it can be passed by
        converting it to an int and letting Cython convert it to what Synthizer needs."""
        protocol = _to_bytes(protocol)
        path = _to_bytes(path)
        cdef void *param_c = <void *>param
        cdef syz_Handle handle
        _checked(syz_createStreamHandleFromStreamParams(&handle, protocol, path, param_c, NULL, NULL))
        return StreamHandle(_handle=handle)

    @staticmethod
    def from_file(path):
        cdef syz_Handle handle
        path = _to_bytes(path)
        _checked(syz_createStreamHandleFromFile(&handle, path, NULL, NULL))
        return StreamHandle(_handle=handle)

    @staticmethod
    def from_memory(const unsigned char[::1] data not None):
        """Build a `StreamHandle` from encoded in-memory data, which can be
        anything implementing Python's buffer protocol over unsigned char (e.g.
        bytes objects, the array module).  This function will keep the data
        alive until the handle is no longer needed on the Synthizer side
        automatically, but changing the data in a way that invalidates the
        buffer will crash and changing the data in other ways (e.g. replacing
        bytes) will have undefined behavior."""
        cdef syz_Handle handle
        cdef const char *ptr
        cdef unsigned long long length
        length = data.shape[0]
        if length == 0:
            raise ValueError("data cannot be of 0 length")
        ptr = <const char *>&data[0]
        _checked(syz_createStreamHandleFromMemory(&handle, length, ptr, NULL, NULL))
        return StreamHandle(_handle = handle)

    @staticmethod
    def from_custom_stream(stream):
        """See register_stream_protocol docstring for what the stream object needs to do."""
        cdef syz_CustomStreamDef callbacks
        cdef syz_Handle handle
        cdef syz_ErrorCode res
        custom_stream_fillout_callbacks(&callbacks, stream)
        _checked(syz_createStreamHandleFromCustomStream(&handle, &callbacks, NULL, NULL))
        return StreamHandle(_handle=handle)

cdef class Generator(Pausable):
    """Base class for all generators."""

    pitch_bend = DoubleProperty(SYZ_P_PITCH_BEND)
    gain = DoubleProperty(SYZ_P_GAIN)

cdef class StreamingGenerator(Generator):
    def __init__(self, _handle = None):
        if _handle is None:
            raise RuntimeError("Use one of the staticmethods to initialize StreamingGenerator in order to specify where the data comes from.")
        super().__init__(_handle)


    @staticmethod
    def from_stream_params(context, protocol, path):
        """Initialize a StreamingGenerator from the stream parameters.

        While this can be used to read files, users are encouraged to use StreamingGenerator.from_file instead.

        For example:
        StreamingGenerator.from_stream_params(protocol = "file", path = "bla.wav")
        """
        cdef syz_Handle ctx
        cdef syz_Handle out
        path = _to_bytes(path)
        protocol = _to_bytes(protocol)
        ctx = context._get_handle_checked(Context)
        _checked(syz_createStreamingGeneratorFromStreamParams(&out, ctx, protocol, path, NULL, NULL, NULL))
        return StreamingGenerator(out)

    @staticmethod
    def from_file(context, path):
        """Initialize a StreamingGenerator from a file."""
        cdef syz_Handle ctx
        cdef syz_Handle out
        path = _to_bytes(path)
        ctx = context._get_handle_checked(Context)
        _checked(syz_createStreamingGeneratorFromFile(&out, ctx, path, NULL, NULL))
        return StreamingGenerator(out)


    @staticmethod
    def from_stream_handle(Context context, StreamHandle stream):
        cdef syz_Handle handle
        _checked(syz_createStreamingGeneratorFromStreamHandle(&handle, context.handle, stream.handle, NULL, NULL))
        return StreamingGenerator(_handle=handle)

    playback_position = DoubleProperty(SYZ_P_PLAYBACK_POSITION)
    looping = IntProperty(SYZ_P_LOOPING, conv_in = int, conv_out = bool)

cdef class Source(Pausable):
    """Base class for all sources."""

    cpdef add_generator(self, generator):
        """Add a generator to this source."""
        cdef syz_Handle h = generator._get_handle_checked(Generator)
        _checked(syz_sourceAddGenerator(self.handle, h))

    cpdef remove_generator(self, generator):
        """Remove a generator from this sourec."""
        cdef syz_Handle h = generator._get_handle_checked(Generator)
        _checked(syz_sourceRemoveGenerator(self.handle, h))

    gain = DoubleProperty(SYZ_P_GAIN)
    filter = BiquadProperty(SYZ_P_FILTER)

cdef class DirectSource(Source) :
    def __init__(self, context):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createDirectSource(&out, ctx, NULL, NULL))
        super().__init__(out)

cdef class AngularPannedSource(Source):
    def __init__(self, context, panner_strategy = PannerStrategy.DELEGATE, azimuth=0.0, elevation=0.0):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createAngularPannedSource(&out, ctx, panner_strategy.value, azimuth, elevation, NULL, NULL))
        super().__init__(out)

    azimuth = DoubleProperty(SYZ_P_AZIMUTH)
    elevation = DoubleProperty(SYZ_P_ELEVATION)

cdef class ScalarPannedSource(Source):
    def __init__(self, context, panner_strategy=PannerStrategy.DELEGATE, panning_scalar=0.0):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createScalarPannedSource(&out, ctx, panner_strategy.value, panning_scalar, NULL, NULL))
        super().__init__(out)

    panning_scalar = DoubleProperty(SYZ_P_PANNING_SCALAR)

cdef class Source3D(Source):
    """A source with 3D parameters."""

    def __init__(self, context, panner_strategy=PannerStrategy.DELEGATE, position=(0.0, 0.0, 0.0)):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createSource3D(&out, ctx, panner_strategy.value, position[0], position[1], position[2], NULL, NULL))
        super().__init__(out)

    distance_model = enum_property(SYZ_P_DISTANCE_MODEL, lambda x: DistanceModel(x))
    distance_ref = DoubleProperty(SYZ_P_DISTANCE_REF)
    distance_max = DoubleProperty(SYZ_P_DISTANCE_MAX)
    rolloff = DoubleProperty(SYZ_P_ROLLOFF)
    closeness_boost = DoubleProperty(SYZ_P_CLOSENESS_BOOST)
    closeness_boost_distance = DoubleProperty(SYZ_P_CLOSENESS_BOOST_DISTANCE)
    position = Double3Property(SYZ_P_POSITION)
    orientation = Double6Property(SYZ_P_ORIENTATION)

cdef class Buffer(_BaseObject):
    """Use Buffer.from_stream_params(protocol, path) to initialize."""

    def __init__(self, _handle = None):
        if _handle is None:
            raise RuntimeError("Use one of the staticmethods to initialize Buffers in order to specify where the data comes from.")
        super().__init__(_handle)

    @staticmethod
    def from_stream_params(protocol, path):
        """Create a buffer from a stream.

        While this function can create buffers from files, users are encouraged to use Buffer.from_file instead."""
        cdef syz_Handle handle
        protocol_b = _to_bytes(protocol)
        path_b = _to_bytes(path)
        cdef char* protocol_c = protocol_b
        cdef char* path_c = path_b
        cdef syz_ErrorCode result
        with nogil:
            result = syz_createBufferFromStreamParams(&handle, protocol_c, path_c, NULL, NULL, NULL)
        _checked(result)
        return Buffer(_handle=handle)

    @staticmethod
    def from_file(path):
        """Create a buffer from a file."""
        cdef syz_Handle handle
        path_b = _to_bytes(path)
        cdef char *path_c = path_b
        cdef syz_ErrorCode result
        with nogil:
            result = syz_createBufferFromFile(&handle, path_c, NULL, NULL)
        # This handles a bunch of stuff to do with converting errors.
        _checked(result)
        return Buffer(_handle=handle)

    @staticmethod
    def from_encoded_data(const unsigned char [::1] data not None):
        """Load a buffer from in-memory audio data.  Works with anything
        implementing the buffer protocol on top of characters, e.g. bytes and
        arrays from the array module.

        This code isn't matching the C API because C and Synthizer usually use
        char for strings, but Python foreces unsigned char on us and Cython is
        limited enough that we can't take both."""
        cdef syz_ErrorCode result
        cdef syz_Handle handle
        cdef const char *ptr
        cdef unsigned long long length
        length = data.shape[0]
        if length == 0:
            raise ValueError("Cannot safely pass empty arrays to Synthizer")
        ptr = <const char *>&data[0]
        with nogil:
            result = syz_createBufferFromEncodedData(&handle, length, ptr, NULL, NULL)
        _checked(result)
        return Buffer(_handle=handle)

    @staticmethod
    def from_float_array(unsigned int sr, unsigned int channels, const float[::1] data not None):
        """Build a buffer from an array of float data."""
        cdef const float *ptr
        cdef unsigned long long length
        cdef syz_Handle handle
        cdef syz_ErrorCode result
        length = data.shape[0]
        if length == 0:
            raise ValueError("Cannot safely pass arrays of 0 length to Synthizer")
        if length % channels != 0:
            raise ValueError("The length of the buffer must be a multiple of the channel count")
        ptr = &data[0]
        with nogil:
            result = syz_createBufferFromFloatArray(&handle, sr, channels, length // channels, ptr, NULL, NULL)
        _checked(result)
        return Buffer(_handle=handle)

    @staticmethod
    def from_stream_handle(StreamHandle stream):
        cdef syz_ErrorCode result
        cdef syz_Handle handle
        cdef syz_Handle stream_handle
        stream_handle = stream.handle
        with nogil:
            result = syz_createBufferFromStreamHandle(&handle, stream_handle, NULL, NULL)
        _checked(result)
        return Buffer(_handle=handle)

    cpdef get_channels(self):
        cdef unsigned int ret
        _checked(syz_bufferGetChannels(&ret, self.handle))
        return ret

    cpdef get_length_in_samples(self):
        cdef unsigned int ret
        _checked(syz_bufferGetLengthInSamples(&ret, self.handle))
        return ret

    cpdef get_length_in_seconds(self):
        cdef double ret
        _checked(syz_bufferGetLengthInSeconds(&ret, self.handle))
        return ret

cdef class BufferGenerator(Generator):
    def __init__(self, context):
        cdef syz_Handle handle
        _checked(syz_createBufferGenerator(&handle, context._get_handle_checked(Context), NULL, NULL))
        super().__init__(handle)

    buffer = ObjectProperty(SYZ_P_BUFFER, Buffer)
    playback_position = DoubleProperty(SYZ_P_PLAYBACK_POSITION)
    looping = IntProperty(SYZ_P_LOOPING, conv_in = int, conv_out = bool)


class NoiseType(Enum):
    UNIFORM = SYZ_NOISE_TYPE_UNIFORM
    VM = SYZ_NOISE_TYPE_VM
    FILTERED_BROWN = SYZ_NOISE_TYPE_FILTERED_BROWN

cdef class NoiseGenerator(Generator):
    def __init__(self, context, channels = 1):
        cdef syz_Handle handle
        _checked(syz_createNoiseGenerator(&handle, context._get_handle_checked(Context), channels, NULL, NULL))
        super().__init__(handle)

    noise_type = enum_property(SYZ_P_NOISE_TYPE, lambda x: NoiseType(x))

cdef class GlobalEffect(_BaseObject):
    cpdef reset(self):
        _checked(syz_effectReset(self.handle))

    gain = DoubleProperty(SYZ_P_GAIN)
    filter_input = BiquadProperty(SYZ_P_FILTER_INPUT)


cdef class EchoTapConfig:
    """An echo tap. Passed to GlobalEcho.set_taps."""
    cdef public float delay
    cdef public float gain_l
    cdef public float gain_r

    def __init__(self, delay, gain_l, gain_r):
        self.delay = delay
        self.gain_l = gain_l
        self.gain_r = gain_r


cdef class GlobalEcho(GlobalEffect):
    def __init__(self, context):
        cdef syz_Handle handle
        _checked(syz_createGlobalEcho(&handle, context._get_handle_checked(Context), NULL, NULL))
        super().__init__(handle)

    cpdef set_taps(self, taps):
        """Takes a list of taps of the form [EchoTap, ...] and sets the taps of the echo."""
        cdef syz_EchoTapConfig *cfgs = NULL
        cdef Py_ssize_t n_taps = len(taps)
        cdef EchoTapConfig tap
        try:
            if n_taps == 0:
                _checked(syz_globalEchoSetTaps(self.handle, 0, NULL))
                return
            cfgs = <syz_EchoTapConfig *>PyMem_Malloc(n_taps * sizeof(syz_EchoTapConfig))
            if not cfgs:
                raise MemoryError()
            for i in range(n_taps):
                t = taps[i]
                cfgs[i].delay = t.delay
                cfgs[i].gain_l = t.gain_l
                cfgs[i].gain_r = t.gain_r
            _checked(syz_globalEchoSetTaps(self.handle, n_taps, cfgs))
        finally:
            PyMem_Free(cfgs)


cdef class GlobalFdnReverb(GlobalEffect):
    def __init__(self, context):
        cdef syz_Handle handle
        _checked(syz_createGlobalFdnReverb(&handle, context._get_handle_checked(Context), NULL, NULL))
        super().__init__(handle)

    mean_free_path = DoubleProperty(SYZ_P_MEAN_FREE_PATH)
    t60 = DoubleProperty(SYZ_P_T60)
    late_reflections_lf_rolloff = DoubleProperty(SYZ_P_LATE_REFLECTIONS_LF_ROLLOFF)
    late_reflections_lf_reference = DoubleProperty(SYZ_P_LATE_REFLECTIONS_LF_REFERENCE)
    late_reflections_hf_rolloff = DoubleProperty(SYZ_P_LATE_REFLECTIONS_HF_ROLLOFF)
    late_reflections_hf_reference = DoubleProperty(SYZ_P_LATE_REFLECTIONS_HF_REFERENCE)
    late_reflections_diffusion = DoubleProperty(SYZ_P_LATE_REFLECTIONS_DIFFUSION)
    late_reflections_modulation_depth = DoubleProperty(SYZ_P_LATE_REFLECTIONS_MODULATION_DEPTH)
    late_reflections_modulation_frequency = DoubleProperty(SYZ_P_LATE_REFLECTIONS_MODULATION_FREQUENCY)
    late_reflections_delay = DoubleProperty(SYZ_P_LATE_REFLECTIONS_DELAY)
