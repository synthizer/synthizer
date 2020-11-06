# GlobalEffect

This is the abstract base class for global effects.

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | 1.0 | value >= 0.0 | The overall gain of the effect.

## Functions

### `syz_effectReset`

```
SYZ_CAPI syz_ErrorCode syz_effectReset(syz_Handle effect);
```

Clears the internal state of the effect. Intended for design/development purposes.  This function may produce clicks and other artifacts and is slow.

## Remarks

All global effects inherit from this object type.
