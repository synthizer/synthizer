# The Context

[Contexts](../object_reference/context.md) represent audio devices and the
listener in 3D space.  They:

- Figure out the output channel format necessary for the current audio device
  and convert audio to it.
- Offer the ability to globally set gain.
- Let users set the position of the listener in 3D space.
- Let users set defaults for other objects, primarily the distance model and
  panning strategies.
  - If your application wants HRTF on by default, it is done on the context by
    setting `SYZ_P_PANNER_STRATEGY`.

For more information on 3D audio, see [the dedicated section](./3d_audio.md).

Almost all objects in Synthizer require a context to be created and must be used
only with the context they're associated with.

A common question is whether an app should ever have more than one context.
Though this is possible, contexts are very expensive objects that directly
correspond to audio devices.  Having 2 or 3 is the upper limit of what is
reasonable, but it is by far easiest to only have one as this prevents running
into issues where you mix objects from different contexts together.