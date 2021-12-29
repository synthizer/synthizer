# Context

## Constructors

### `syz_createContext`

```
SYZ_CAPI syz_ErrorCode syz_createContext(syz_Handle *out, void *userdata, syz_UserdataFreeCallback *userdata_free_callback);
```

Creates a context configured to play through the default output device.

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | 1.0 | value >= 0.0 | The gain of the context
SYZ_P_POSITION | double3 | (0, 0, 0) | any | The position of the listener.
SYZ_P_ORIENTATION | double6 | (0, 1, 0, 0, 0, 1) | Two packed unit vectors | The orientation of the listener as `(atx, aty, atz, upx, upy, upz)`.
SYZ_P_DEFAULT_DISTANCE_MODEL | int | SYZ_DISTANCE_MODEL_LINEAR | any SYZ_DISTANCE_MODEL | The default distance model for new sources.
SYZ_P_DEFAULT_DISTANCE_REF | double | 1.0 | value >= 0.0 | The default reference distance for new sources.
SYZ_P_DEFAULT_DISTANCE_MAX | double | 50.0 | value >= 0.0 | The default max distance for new sources.
SYZ_P_DEFAULT_ROLLOFF | double | 1.0 | value >= 0.0 | The default rolloff for new sources.
SYZ_P_DEFAULT_CLOSENESS_BOOST | double | 0.0 | any finite double | The default closeness boost for new sources in DB.
SYZ_P_DEFAULT_CLOSENESS_BOOST_DISTANCE | double | 0.0 | value >= 0.0 | The default closeness boost distance for new sources
SYZ_P_DEFAULT_PANNER_STRATEGY | int | SYZ_PANNER_STRATEGY_STEREO | any SYZ_PANNER_STRATEGY | The default panner strategy for new sources.

## Functions
### `syz_contextGetNextEvent`

See [events](../concepts/events.md).

## Linger Behavior

None.

## Remarks

The context is the main entrypoint to Synthizer, responsible for the following:

- Control and manipulation of the audio device.
- Driving the audio threads.
- Owning all objects that play together.
- Representing the listener in 3D space.

All objects which are associated with a context take a context as part of all
their constructors.  Two objects which are both associated with different
contexts should never interact. For efficiency, whether two objects are from
different contexts is unvalidated, and the behavior of mixing them is undefined.

All objects associated with a context become useless once the context is
destroyed.  Calls to them will still work, but they can't be reassociated with a
different context and no audible output will result.

Most programs create one context and destroy it at shutdown.

For the time being, all contexts output stereo audio, and it is not possible to
specify the output device. These restrictions will be lifted in future.

For information on the meaning of the distance model properties, see [3D
Audio](../concepts/3d_audio.md).
