import contextlib

from synthizer_properties cimport *
from synthizer_constants cimport *

cdef class SynthizerError(Exception):
    cdef str message
    cdef int code

    def __init__(self):
        self.code = syz_getLastErrorCode()
        cdef char *tmp = syz_getLastErrorMessage()
        if tmp == NULL:
            self.message = ""
        else:
            self.message = str(tmp, "utf-8")

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

    def __set__(self, _BaseObject instance, int value):
        _checked(syz_setI(instance.handle, self.property, self.conv_in(value)))

def enum_property(v, e):
    return IntProperty(v, conv_in = lambda x: int(x), conv_out = lambda x: e(x))

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

cpdef enum LogLevel:
    ERROR = SYZ_LOG_LEVEL_ERROR
    WARN = SYZ_LOG_LEVEL_WARN
    INFO = SYZ_LOG_LEVEL_INFO
    DEBUG = SYZ_LOG_LEVEL_DEBUG

cpdef enum LoggingBackend:
    STDERR = 0

cpdef configure_logging_backend(LoggingBackend backend):
    _checked(syz_configureLoggingBackend(<SYZ_LOGGING_BACKEND>backend, NULL))

cpdef set_log_level(LogLevel level):
    syz_setLogLevel(<SYZ_LOG_LEVEL>level)


cpdef initialize():
    """Initialize Synthizer.  Try synthizer.Initialized for a context manager that will shut things down on exceptions. """
    _checked(syz_initialize())

cpdef shutdown():
    """Shut Synthizer down."""
    _checked(syz_shutdown())

@contextlib.contextmanager
def initialized():
    """A context manager that safely initializes and shuts Synthizer down.

    Usage:

    with synthizer.initialized():
        ...my code
    """
    initialize()
    try:
        yield
    except BaseException as e:
        raise e
    finally:
        shutdown()

cpdef enum PannerStrategy:
    HRTF = SYZ_PANNER_STRATEGY_HRTF

cpdef enum DistanceModel:
    NONE = SYZ_DISTANCE_MODEL_NONE
    LINEAR = SYZ_DISTANCE_MODEL_LINEAR
    EXPONENTIAL = SYZ_DISTANCE_MODEL_EXPONENTIAL
    INVERSE = SYZ_DISTANCE_MODEL_INVERSE

cdef class _BaseObject:
    cdef syz_Handle handle

    def __init__(self, int handle):
        self.handle = handle

    cdef destroy(self):
        """Destroy this object.  This function is useful to ensure destruction of resources immediately as opposed to waiting for the garbage collector."""
        _checked(syz_handleDecRef(self.handle))
        self.handle = 0

    cpdef syz_Handle _get_handle_checked(self, cls):
        #Internal: get a handle, after checking that this object is a subclass of the specified class.
        if not isinstance(self, cls):
            raise ValueError("Synthizer object is of an unexpected type")
        return self.handle

    def __dealloc__(self):
        if self.handle != 0:
            syz_handleDecRef(self.handle)


class Context(_BaseObject):
    """The Synthizer context represents an open audio device and groups all Synthizer objects created with it into one unit.

    To use Synthizer, the first step is to create a Context, which all other library types expect in their constructors.  When the context is destroyed, all audio playback stops and calls to
    methods on other objects in Synthizer will generally not have an effect.  Once the context is gone, the only operation that makes sense for other object types
    is to destroy them.  Put another way, keep the Context alive for the duration of your application."""

    def __init__(self):
        cdef syz_Handle handle
        _checked(syz_createContext(&handle))
        super().__init__(handle)

    listener_position = Double3Property(SYZ_CONTEXT_LISTENER_POSITION)
    listener_orientation = Double6Property(SYZ_CONTEXT_LISTENER_ORIENTATION)
    distance_model = enum_property(SYZ_CONTEXT_DISTANCE_MODEL, lambda x: DistanceModel(x))
    distance_ref = DoubleProperty(SYZ_CONTEXT_DISTANCE_REF)
    distance_max = DoubleProperty(SYZ_CONTEXT_DISTANCE_MAX)
    rolloff = DoubleProperty(SYZ_CONTEXT_ROLLOFF)
    closeness_boost = DoubleProperty(SYZ_CONTEXT_CLOSENESS_BOOST)
    closeness_boost_distance = DoubleProperty(SYZ_CONTEXT_CLOSENESS_BOOST_DISTANCE)


cdef class Generator(_BaseObject):
    """Base class for all generators."""
    pass

cdef class StreamingGenerator(Generator):
    def __init__(self, context, protocol, path, options = ""):
        """Initialize a StreamingGenerator.

        For example:
        StreamingGenerator(protocol = "file", path = "bla.wav")

        Options is currently unused and will be better documented once it is.
        """
        cdef syz_Handle ctx
        cdef syz_Handle out
        path = _to_bytes(path)
        protocol = _to_bytes(protocol)
        options = _to_bytes(options)
        ctx = context._get_handle_checked(Context)
        _checked(syz_createStreamingGenerator(&out, ctx, protocol, path, options))
        super().__init__(out)


cdef class Source(_BaseObject):
    """Base class for all sources."""

    cpdef add_generator(self, generator):
        """Add a generator to this source."""
        cdef syz_Handle h = generator._get_handle_checked(Generator)
        _checked(syz_sourceAddGenerator(self.handle, h))

    cpdef remove_generator(self, generator):
        """Remove a generator from this sourec."""
        cdef syz_Handle h = generator._get_handle_checked(Generator)
        _checked(syz_sourceRemoveGenerator(self.handle, h))

cdef class PannedSourceCommon(Source):
    """Properties common to PannedSource and SOurce3D"""
    panner_strategy = enum_property(SYZ_PANNED_SOURCE_PANNER_STRATEGY, lambda x: PannerStrategy(x))
    gain = DoubleProperty(SYZ_PANNED_SOURCE_GAIN)

cdef class PannedSource(PannedSourceCommon):
    """A source with azimuth and elevation panning done by hand."""

    def __init__(self, context):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createPannedSource(&out, ctx))
        super().__init__(out)

    azimuth = DoubleProperty(SYZ_PANNED_SOURCE_AZIMUTH)
    elevation = DoubleProperty(SYZ_PANNED_SOURCE_ELEVATION)
    panning_scalar = DoubleProperty(SYZ_PANNED_SOURCE_PANNING_SCALAR)


cdef class Source3D(PannedSourceCommon):
    """A source with 3D parameters."""

    def __init__(self, context):
        cdef syz_Handle ctx = context._get_handle_checked(Context)      
        cdef syz_Handle out
        _checked(syz_createSource3D(&out, ctx))
        super().__init__(out)

    distance_model = enum_property(SYZ_SOURCE3D_DISTANCE_MODEL, lambda x: DistanceModel(x))
    distance_ref = DoubleProperty(SYZ_SOURCE3D_DISTANCE_REF)
    distance_max = DoubleProperty(SYZ_SOURCE3D_DISTANCE_MAX)
    rolloff = DoubleProperty(SYZ_SOURCE3D_ROLLOFF)
    closeness_boost = DoubleProperty(SYZ_SOURCE3D_CLOSENESS_BOOST)
    closeness_boost_distance = DoubleProperty(SYZ_SOURCE3D_CLOSENESS_BOOST_DISTANCE)
    position = Double3Property(SYZ_SOURCE3D_POSITION)
    orientation = Double6Property(SYZ_SOURCE3D_ORIENTATION)
