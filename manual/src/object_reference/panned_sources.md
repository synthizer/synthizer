# AngularPannedSource and ScalarPannedSource

## Constructors

### `syz_createAngularPannedSource`

```
SYZ_CAPI syz_ErrorCode syz_createAngularPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                     double azimuth, double elevation, void *userdata,
                                                     syz_UserdataFreeCallback *userdata_free_callback);
```

Creates an angular panned source, which can be controled through azimuth and elevation.

### `syz_createScalarPannedSource`

```
SYZ_CAPI syz_ErrorCode syz_createScalarPannedSource(syz_Handle *out, syz_Handle context, int panner_strategy,
                                                    double panning_scalar, void *userdata,
                                                    syz_UserdataFreeCallback *userdata_free_callback);
```

Creates a scalar panned source, controlled via the panning scalar.

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_AZIMUTH | double | 0.0 | 0.0 to 360.0 | The azimuth of the panner. See remarks.
SYZ_P_ELEVATION | double | 0.0 | -90.0 to 90.0 | See remarks
SYZ_P_PANNING_SCALAR | double | 0.0 | -1.0 to 1.0 | see remarks

## Linger Behavior

Lingers until all generators have been destroyed.

## Remarks

The panned sources give direct control over a panner, which is either controlled via azimuth/elevation in degrees or a
panning scalar.  Which properties you use depend on which type of source you create (angular for azimuth/elevation,
scalar for the panning scalar).

If using azimuth/elevation, 0.0 azimuth is forward and positive angles are clockwise.  Elevation ranges from -90 (down)
to 90 (up).

Some applications want to control panners through a panning scalar instead, i.e. for UI purposes. If using panning
scalars, -1.0 is full left and 1.0 is full right.

For information on panning, see [3D Audio](../concepts/3d_audio.md).
