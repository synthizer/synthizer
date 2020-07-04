# The Signal Graph, Library Parameters, and Limitations

Synthizer's signal graph is/will be as follows (not all of this exists yet):

- 1 or more generators feed a source, optionally through a number of effects.
- 1 or more sources feed:
  - A context, which is directly passed to audio output.
  - Optionally, any number of user-specified effects.
  - Optionally, any number of reverb zones.

Synthizer always processes audio at a sample rate of 44100, and with a block size of 256. All parameter updates take effect
at the block boundaries, or around every 5 MS.  These parameters cannot be reconfigured without recompiling the library.  Synthizer will get the best latency it can for the system it's running on.  This can also not be controlled or influenced by the programmer.

No audio input or output can be over 16 channels.  This limit may be raised in future.

Object handles are not reused, but there is a limit of 65535 concurrent outstanding object handles. This is due to an internal lockfree
slab which allows for relatively fast handle translation without syscalls.  If this limit proves problematic, it will be raised, but for all practical intents memory or CPU will be exhausted first.
