# NoiseGenerator

Inherits from [Generator](./generator.md).

## Constructors

### `syz_createNoiseGenerator`

```
SYZ_CAPI syz_ErrorCode syz_createNoiseGenerator(syz_Handle *out, syz_Handle context, unsigned int channels);
```

Creates a NoiseGenerator configured for uniform noise with the specified number of output channels.
The number of output channels cannot be configured at runtime.  Each channel produces decorrelated noise.


## Properties

Enum | Type | Default Value | Range | Description
--- | --- | --- | --- | ---
SYZ_P_NOISE_TYPE | int | SYZ_NOISE_TYPE_UNIFORM | any SYZ_NOISE_TYPE | The type of noise to generate. See remarks.

## Remarks

NoiseGenerators generate noise, which will be useful in future when various effects are added.  For instance filtered noise makes plausible wind.
Synthizer allows setting the algorithm used to generate noise to one of the following options.  Note that these are more precisely named than white/pink/brown; the sections below document the equivalent in the more standard nomenclature.

### `SYZ_NOISE_TYPE_UNIFORM`

A uniform noise source.  From an audio perspective this is white noise, but is sampled from a uniform rather than Gaussian distribution for efficiency.

### `SYZ_NOISE_TYPE_VM`

This is pink noise generated with the Voss-McCartney algorithm, which consists of a number of summed uniform random number generators which are run at different rates.
Synthizer adds an additional random number generator at the top of the hierachy in order to improve the color of the noise in the high frequencies.

### `SYZ_NOISE_TYPE_FILTERED_BROWN`

This is brown noise generated with a -6DB filter.
