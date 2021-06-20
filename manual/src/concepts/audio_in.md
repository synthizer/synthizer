# basics of Audio in Synthizer

This section explains how to get audio into and out of Synthizer.  The following
objects must be used by every application:

- Generators produce audio, for example by reading a buffer of audio data.
- Sources play audio from one or more generators.
- Contexts represent audio devices and group objects for the same device
  together.

The most basic flow of Synthizer is to create a context, source, and generator,
then connect the generator to the source.  For example, you might combine
[BufferGenerator](../object_reference/buffer_generator.md) and
[DirectSource](../object_reference/direct_source.md) to play a stereo audio file
to the speakers, or swap `DirectSource` for
[Source3D](../object_reference/source_3d.md) to place the sound in the 3D
environment.
