# Introduction to Sources

Sources are how generators are made audible.  Synthizer offers 3 main kinds of
source:

- The [DirectSource](../object_reference/direct_source.md) plays audio directly,
  and can be used for things like background music.  This is the only source
  type which won't convert audio to mono before using it.
- The [PannedSource](../object_reference/panned_source.md) allows for manual
  control over pan, either by azimuth/elevation or via a scalar from -1 to 1
  where -1 is all left and 1 is all right.
- The [Source3D](../object_reference/source_3d.md) allows for positioning audio
  in 3D space.

Every source offers the following functions:

```
SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);
```

Every source will mix audio from as many generators as are connected to it and
then feed the audio through to the output of the source and to effects.  See the
section on [channel mixing](./channel_mixing.md) for how audio is converted to
various different output formats, and [effects and
filters](./filters_and_effects.md) for information on how to do more with this
API than simply playing audio.
