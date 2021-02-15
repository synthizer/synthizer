from enum import Enum
from typing import Iterator, List, Optional, Tuple, Union

StringOrBytes = Union[str, bytes]
Double3 = Tuple[float, float, float]
Double6 = Tuple[float, float, float, float, float, float]
EventSourceType = Union['Source', 'Generator']


class LoggingBackend(Enum):
    STDERR: int


STDERR: LoggingBackend = LoggingBackend.STDERR


class LogLevel(Enum):
    DEBUG: int
    ERROR: int
    INFO: int
    WARN: int


DEBUG: LogLevel = LogLevel.DEBUG
INFO: LogLevel = LogLevel.INFO
ERROR: LogLevel = LogLevel.ERROR
WARN: LogLevel = LogLevel.WARN


class DistanceModel(Enum):
    NONE: int
    LINEAR: int
    EXPONENTIAL: int
    INVERSE: int


NONE: DistanceModel = DistanceModel.NONE
LINEAR: DistanceModel = DistanceModel.LINEAR
EXPONENTIAL: DistanceModel
INVERSE: DistanceModel


class NoiseType(Enum):
    FILTERED_BROWN: int
    UNIFORM: int
    VM: int


FILTERED_BROWN: NoiseType = NoiseType.FILTERED_BROWN
UNIFORM: NoiseType = NoiseType.UNIFORM
VM: NoiseType = NoiseType.VM


class PannerStrategy(Enum):
    HRTF: int
    STEREO: int


HRTF: PannerStrategy = PannerStrategy.HRTF
STEREO: PannerStrategy = PannerStrategy.STEREO

# Initialization functions:


def initialized() -> Iterator[None]: ...


def initialize() -> None: ...


def shutdown() -> None: ...


# Functions for configuring logging:


def configure_logging_backend(backend: LoggingBackend) -> None: ...


def set_log_level(level: LogLevel) -> None: ...


class _BaseObject:
    def __init__(self, _handle: int) -> None: ...

    def destroy(self) -> None: ...

    def get_userdata(self) -> object: ...

    def set_userdata(self, data: object) -> None: ...


class Pausable(_BaseObject):
    def pause(self) -> None: ...

    def play(self) -> None: ...


class Buffer(_BaseObject):
    @staticmethod
    def from_stream(
        protocol: StringOrBytes, path: StringOrBytes,
        options: StringOrBytes = ''
    ) -> 'Buffer': ...

    def get_channels(self) -> int: ...

    def get_length_in_samples(self) -> int: ...

    def get_length_in_seconds(self) -> float: ...


class Generator(Pausable):
    gain: float = ...
    pitch_bend: float = ...


class Source(Pausable):
    gain: float = 1.0

    def add_generator(self, generator: Generator) -> None: ...

    def remove_generator(self, generator: Generator) -> None: ...


class GlobalEffect(_BaseObject):
    gain: float = 1.0

    def reset(self) -> None: ...


class Context(Pausable):
    closeness_boost: float = 0.0
    closeness_boost_distance: float = 0.0
    distance_max: float = 50.0
    distance_model: DistanceModel = LINEAR
    distance_ref: float = 1.0
    gain: float = 1.0
    orientation: Double6 = (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    panner_strategy: PannerStrategy = STEREO
    position: Double3 = (0.0, 0.0, 0.0)
    rolloff: float = 1.0

    def __init__(self, enable_events: bool = False) -> None: ...

    def config_route(
        self, output: Source, input: GlobalEffect, gain: float = 1.0,
        fade_time: float = 0.01
    ) -> None: ...

    def enable_events(self) -> None: ...

    def get_events(self, limit: int = 0) -> Iterator['Event']: ...

    def remove_route(
        self, output: Source, input: GlobalEffect, fade_time: float = 0.01
    ) -> None: ...


class DirectSource(Source):
    def __init__(self, context: 'Context') -> None: ...


class PannedSourceCommon(Source):
    panner_strategy: PannerStrategy = STEREO


class PannedSource(PannedSourceCommon):
    azimuth: float = 0.0
    elevation: float = 0.0
    panning_scalar: float = 0.0

    def __init__(self, context: 'Context') -> None: ...


class Source3D(PannedSourceCommon):
    closeness_boost: float = 0.0
    closeness_boost_distance: float = 0.0
    distance_max: float = 50.0
    distance_model: DistanceModel = LINEAR
    distance_ref: float = 1.0
    orientation: Double6 = (0.0, 1.0, 0.0, 0.0, 0.0, 1.0)
    position: Double3 = (0.0, 0.0, 0.0)
    rolloff: float = 1.0

    def __init__(self, context: 'Context') -> None: ...


class BufferGenerator(Generator):
    buffer: Optional[Buffer] = None
    looping: bool = False
    position: float = 0.0

    def __init__(self, context: Context) -> None: ...


class EchoTapConfig:
    delay: float
    gain_l: float
    gain_r: float

    def __init__(self, delay: float, gain_l: float, gain_r: float) -> None: ...


class Event:
    context: Context
    source: Optional[EventSourceType]

    def __init__(
        self, context: Context, source: Optional[EventSourceType]
    ) -> None: ...


class FinishedEvent(Event):
    pass


class LoopedEvent(Event):
    pass


class GlobalEcho(GlobalEffect):
    def __init__(self, context: Context) -> None: ...

    def set_taps(self, taps: List[EchoTapConfig]) -> None: ...


class GlobalFdnReverb(GlobalEffect):
    late_reflections_delay: float = 0.01
    late_reflections_diffusion: float = 1.0
    late_reflections_hf_reference: float = 500.0
    late_reflections_hf_rolloff: float = 0.5
    late_reflections_lf_reference: float = 200.0
    late_reflections_lf_rolloff: float = 1.0
    late_reflections_modulation_depth: float = 0.01
    late_reflections_modulation_frequency: float = 0.5
    mean_free_path: float = 0.02
    t60: float = 1.0

    def __init__(self, context: Context) -> None: ...


class NoiseGenerator(Generator):
    noise_type: NoiseType = UNIFORM

    def __init__(self, context: Context, channels: int = 1) -> None: ...


class StreamingGenerator(Generator):
    looping: bool = False
    position: float = 0.0

    def __init__(
        self, context: Context, protocol: StringOrBytes, path: StringOrBytes,
        options: StringOrBytes = ''
    ) -> None: ...


class SynthizerError(Exception):
    ...
