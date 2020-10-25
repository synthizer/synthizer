# The Signal Graph, Library Parameters, and Limitations

Synthizer's signal graph is as follows:

- 1 or more generators feed a source.
- 1 or more sources feed:
  - A context, which is directly passed to audio output.
  - Optionally, any number of global effects.

Synthizer always processes audio at a sample rate of 44100, and with a fixed block size. All parameter updates take effect
at the block boundaries.  These parameters cannot be reconfigured without recompiling the library.  Synthizer will get the best latency it can for the system it's running on.  This can also not be controlled or influenced by the programmer.

Currently, the block size is 256 samples, or around 5 MS.  The authoritative source for this number is `include/synthizer/config.hpp` in the Synthizer repository.  The hope is that this may be lowered to 128 or better in future.

No audio input or output can be over 16 channels.  This limit may be raised in future.

Object handles are not reused, but there is a limit of 65535 concurrent outstanding object handles. This is due to an internal lockfree
slab which allows for relatively fast handle translation without syscalls.  If this limit proves problematic, it will be raised, but for all practical intents memory or CPU will be exhausted first.
