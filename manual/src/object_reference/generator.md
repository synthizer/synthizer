# Operations Common to All Generators

Generators generate audio, and are how Synthizer knows what to play through
sources. 

## Properties

All generators support the following properties:

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | 1.0 | value >= 0.0 | The gain of the generator.
SYZ_P_PITCH_BEND | double | 1.0 | 0.0 <= value <= 2.0 | Pitch bend of the generator as a multiplier (2.0 is +1 octave, 0.5 is -1 octave, etc)

## Remarks

Not all generators support `SYZ_P_PITCH_BEND` because it doesn't necessarily
make sense for them to do so, but it can always be set.
