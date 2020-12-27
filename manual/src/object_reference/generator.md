# Generator (abstract)

Generators generate audio, and are how Synthizer knows what to play through sources. In addition to direct
generation, some generators take other generators as arguments, e.g. per-source effects and filters.

## Properties

All generators support the following properties:

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | 1.0 | value >= 0.0 | The gain of the generator.
SYZ_P_PITCH_BEND | double | 1.0 | value >= 0.0 | Pitch bend of the generator as a multiplier (2.0 is +1 octave, 0.5 is -1 octave, etc)

## Functions

### `syz_pause`, `syz_play`

```
syz_ErrorCode syz_pause(syz_Handle object);
syz_ErrorCode syz_play(syz_Handle object);

```

The standard play/pause functions, which do exactly what their name suggests.

## Remarks

Not all generators support `SYZ_P_PITCH_BEND` because it doesn't necessarily make sense for them to do so.  Where this is the case, this manual
will document that in the remarks for that generator type.  Additionally, in cases where
`SYZ_P_PITCH_BEND` has non-obvious behavior, the remarks will document that as well.  The most common place to se non-obvious `SYZ_P_PITCH_BEND` behavior is in effects.
