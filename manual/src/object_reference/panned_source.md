# PannedSource

## Constructors

### `syz_createPannedSource`

```
SYZ_CAPI syz_ErrorCode syz_createPannedSource(syz_Handle *out, syz_Handle context);
```

Creates a panned source.

### Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_AZIMUTH | double | 0.0 | 0.0 to 360.0 | The azimuth of the panner. See remarks.
SYZ_P_ELEVATION | double | 0.0 | -90.0 to 90.0 | See remarks
SYZ_P_PANNING_SCALAR | double | 0.0 | -1.0 to 1.0 | see remarks

# Remarks

The PannedSource gives direct control over a panner, which is either controlled
via azimuth/elevation in degrees or a panning scalar.

If using azimuth/elevation, 0.0 azimuth is forward and positive angles are
clockwise.  Elevation ranges from -90 (down) to 90 (up).

Some applications want to control panners through a panning scalar instead, i.e.
for UI purposes. If using panning scalars, -1.0 is full left and 1.0 is full
right.

Applications should use either a panning scalar or azimuth/elevation, never both
on the same source.  Using both simultaneously is undefined behavior.

For information on panning, see [3D Audio](../concepts/3d_audio.md).
