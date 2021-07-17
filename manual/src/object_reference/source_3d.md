# Source3D

## Constructors

### `syz_createSource3D`

```
SYZ_CAPI syz_ErrorCode syz_createSource3D(syz_Handle *out, syz_Handle context, void *userdata, syz_UserdataFreeCallback *userdata_free_callback);
```

Creates a source3d positioned at the origin and with no associated generators.

## Properties
Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_POSITION | double3 | (0, 0, 0) | any | The position of the source.
SYZ_P_ORIENTATION | double6 | (0, 1, 0, 0, 0, 1) | Two packed unit vectors | The orientation of the source as `(atx, aty, atz, upx, upy, upz)`. Currently unused.
SYZ_P_DISTANCE_MODEL | int | from Context | any SYZ_DISTANCE_MODEL | The distance model for this source.
SYZ_P_DISTANCE_REF | double | From Context | value >= 0.0 | The reference distance.
SYZ_P_DISTANCE_MAX | double | From Context | value >= 0.0 | The max distance for this source.
SYZ_P_ROLLOFF | double | From Context | value >= 0.0 | The rolloff for this source.
SYZ_P_CLOSENESS_BOOST | double | From Context | any finite double | The closeness boost for this source in DB.
SYZ_P_CLOSENESS_BOOST_DISTANCE | double | From Context | value >= 0.0 | The closeness boost distance for this source.


## Remarks

A Source3D represents an entity in 3D space.  For explanations of the above
properties, see [3D Audio](../concepts/3d_audio.md).

When created, Source3D reads all of its defaults from the Context's
corresponding properties.  Changes to the Context versions don't affect already
created sources.  A typical use case is to configure the Context to the defaults
of the game, and then create sources.
