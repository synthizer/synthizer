# A Basic Python Tutorial

## Installation

We support Python 3.  Python wheels for Windows X64 are published to Pypi, so to install:

```bash
pip install synthizer
```

## Some Basics

Synthizer requires initialization and deinitialization. The best way to initialize Synthizer is to use the context manager:

```python
with synthizer.initialized():
    # Code...
```

The context manager will handle deinitialization on exceptions.  Though `synthizer.initialize()` and `synthizer.shutdown()` are exposed, use is discouraged as the context manager ensures clean shutdown in (almost) all cases.

Once the library is deinitialized, calls into Synthizer error.

## Destroying Objects

Unlike the rest of Python, Synthizer objects represent concrete audio architecture, which need to have a defined lifetime in order to control when they are and aren't being heard,
and also to ensure that it can be well understood when things are and aren't using system resources.  To that end, Synthizer objects don't interact with Python garbage collection. When you're done with an object, call `myobj.destroy()`.

At the moment, failing to do so will permanently leak the object.  Work in future may lift this restriction, but it's still necessary to be explicit: if you aren't, things may be audioble longer than you intend.

## The Context

Most objects in Synthizer require a context, which represents a listener in 3D space, an audio output device, and other miscellaneous infrastructure.
Objects are passed a context on construction, and two objects from different contexts never interact.

To get a context:

```python
ctx = synthizer.Context()
```

Currently, selecting audio output devices isn't supported, and audio will go to the system default.

## Buffers, Streams, and Generators

As elaborated in the concepts section, the Synthizer audio graph is as follows:

- A source is fed by one or more generators and pans audio.
- All the sources feed the context's output.

Generators are an abstract concept, which represents somewhere audio comes from. Specific kinds of generators implement the abstract interface in a concrete fashion,
notably `BufferGenerator` (takes a buffer) and `StreamingGenerator` (takes streaming parameters).

The easiest way to get audio into Synthizer is via streams.  Streams are specified as pre-parsed URL-like components:

- A protocol, `file` for example, which specifies where the audio comes from.
- A path, which specifies where the audio is (i.e. path on disk, etc).
- Options, of the form `key=value&key=value&...`.  At the moment, unused by anything, so just use `""`.

To play back a stream, you have two choices: you can use a `StreamingGenerator`, which takes these parameters directly and will decode in realtime, or you can use a `BufferGenerator`, which takes a pre-decoded buffer that you made previously from a strem specification.
You use `StreamingGenerator` for things like music and `BufferGenerator` for things like short sounds. Note that
`StreamingGenerator` is expensive and relatively high latency.

Buffers are in-memory decoded assets, essentially arrays of 16-bit samples resampled to Synthizer's samplerate. Note that they aren't actually contiguous arrays and are also immutable.

To get a streaming generator:

```python
generator = synthizer.StreamingGenerator(ctx, "file", "test.wav")
```

To get a buffer from a stream:

```python
buffer = synthizer.Buffer.from_stream("file", "test.wav")
```

Note the following:

- Buffers aren't associated with a context and can be used anywhere,.
- Buffers can be used simultaneously by multiple generators (or whatever else). You are encouraged to cache them and reuse them indefinitely.
- When Synthizer needs to overload a constructor, it does so as staticmethods on the class. We don't have
  multiple constructors here yet, but will in future.

## Sources

Sources represent audio output.  They get one or more generators, combine them all, and output.  Currently we have the following kinds of sources:

- A PannedSource is manually controlled using azimuth and elevation or, alternatively, a panning scalar.
- Source3D is a 3D environmental source with the usual things you'd expect: distance model, position, etc.

## Properties

Synthizer offers the following kinds of properties:

- int, represented as either a Python integer type or an enum. An example of the latter case is `source.distance_model`, which is a `synthizer.DistanceModel`. Synthizer exposes enums as Python 3.4-style enums via Cython.
- Double, which is self-explanatory.
- Double3, which is a tuple of 3 doubles. Usually used as a position.
- Double6, a tuple of 6 doubles. Usually used as an orientation (given a dedicated section below).
- Object, i.e. `buffer_generator.buffer = b`.

It is important to note that Synthizer properties are eventually consistent.  What this means is that code like the following doesn't do what you expect:

```python
myobj.property = 0
myobj.property += 5
myobj.property += 5
# May or may not fail, depending on timing.
assert myobj.property == 10

myobj.property = 15
x = myobj.property
# may or may not fail depending on timing.
assert x == 15
```

Property reads are primarily useful for properties like position on various generators, where Synthizer is updating the property itself.
In general, it's best to use properties to tell Synthizer what to do, but keep the model of what's supposed to be going on in your code.
A common mistake is to try to use Synthizer to store data, for example putting the position of your objects in a source rather than maintaing the coordinates yourself.

Object properties are internally referenced in a weak fashion. That is to say that destroying (`.destroy()`)the object the property is set to will clear the property. 

In the Python bindings, Synthizer translates `SYZ_P_MY_PROPERTY` to `obj.my_property`. The translations of the tables in the object reference are nearly mechanical, and this simple
transformation always tells you where the property lives in Python.

## An aside: orientation formulas

Before going much further, most people seem to eventually ask about trigonometry with respect to using a 3D audio library for 2D games. The high level overview
for those who already know trigonometry is that Synthizer's coordinate system is right-handed and orientations consist of 2 orthogonal unit vectors `(atx, aty, atz, upx, upy, upz)` stored
as a packed property so that they can both be set atomically.  But the longer version for those who don't know trigonometry is:

Degrees to radians is:

```python
import math

def deg2rad(angle):
    return (angle / 180.0) * math.pi
```

People who don't know trig usually ask for orientations that are clockwise of north. To do that:

```python
import math

def make_orientation(degrees):
    rad = deg2rad(angle)
    return (math.sin(rad), math.cos(rad), 0, 0, 0, 1)
```

Setting `context.orientation` to the result of the above will set things up so that you can treat positive x as east, positive y as north, and positive z as up. The default orientation faces the listener north.

## Putting it Together

To play a source in 3D space, do the following:

- Create a context.
- Create a source.
- Create a buffer.
- Create a generator.
- `generator.buffer = buffer`
- `source.add_generator(generator)`
- Then manipulate position, etc. on the source.

## A Worked Example

The following is a 3D media player, the audio library equivalent of hello world. It supports the following commands:

- `pause`, `play`: pause/play the source
- `seek <seconds>`: self-explanatory.
- `pos <x> <y> <z>`: move the source. X is right, y is forward, z is up.
- `loop`: Toggle looping of the generator.
- `gain <value>`: Control the gain of the generator, in DB.
- `quit`: self-explanatory.

Note that the default distance model parameters cause the source to become completely silent at around 50 units out. Movements close to the head won't change the volume much. Also, HRTF improvements are coming.

This example also doesn't demonstrate destruction, as that's handled by library deinitialization and process shutdown. A proper program needs `source.destroy()` etc for dynamic sources, as explained above.

The code:

```python
"""A simple media player, demonstrating the Synthizer basics."""

import sys

import synthizer

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <file>")
    sys.exit(1)

# Log to debug. At the moment this writes directly to stdout, but will in
#  future integrate with Python's logging modules.
# It's best to call this before any initialization.
synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

with synthizer.initialized():
    # Get our context, which almost everything requires.
    # This starts the audio threads.
    ctx = synthizer.Context()

    # A BufferGenerator plays back a buffer:
    generator = synthizer.BufferGenerator(ctx)
    # A buffer holds audio data. We read from the specified file:
    buffer = synthizer.Buffer.from_stream("file", sys.argv[1])
    # Tell the generator to use the buffer.
    generator.buffer = buffer
    # A Source3D is a 3D source, as you'd expect.
    source = synthizer.Source3D(ctx)
    # It'll play the BufferGenerator.
    source.add_generator(generator)
    # Keep track of looping, since property reads are expensive:
    looping = False

    # A simple command parser.
    while True:
        cmd = input("Command: ")
        cmd = cmd.split()
        if len(cmd) == 0:
            continue
        if cmd[0] == "pause":
            source.pause()
        elif cmd[0] == "play":
            source.play()
        elif cmd[0] == "pos":
            if len(cmd) < 4:
                print("Syntax: pos x y z")
                continue
            try:
                x, y, z = [float(i) for i in cmd[1:]]
            except ValueError:
                print("Unable to parse coordinates")
                continue
            source.position = (x, y, z)
        elif cmd[0] == "seek":
            if len(cmd) != 2:
                print("Syntax: pos <seconds>")
                continue
            try:
                pos = float(cmd[1])
            except ValueError:
                print("Unable to parse position")
                continue
            try:
                generator.position = pos
            except synthizer.SynthizerError as e:
                print(e)
        elif cmd[0] == "quit":
            break
        elif cmd[0] == "loop":
            looping = not looping
            generator.looping = looping
            print("Looping" if looping else "Not looping")
        elif cmd[0] == "gain":
            if len(cmd) != 2:
                print("Syntax: gain <value>")
                continue
            try:
                value = float(cmd[1])
            except ValueError:
                print("Unable to parse value.")
                continue
            # Convert to scalar gain from db.
            gain = 10 ** (value / 20)
            source.gain = gain
        else:
            print("Unrecognized command")
```
