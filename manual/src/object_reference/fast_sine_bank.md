# FastSineBankGenerator

Generate basic waveforms which can be expressed as the sum of sine waves (e.g. square, triangle).

## Constructors

### `syz_createFastSineBankGenerator`

```
struct syz_SineBankWave {
  double frequency_mul;
  double phase;
  double gain;
};

struct syz_SineBankConfig {
  const struct syz_SineBankWave *waves;
  unsigned long long wave_count;
  double initial_frequency;
};

SYZ_CAPI void syz_initSineBankConfig(struct syz_SineBankConfig *cfg);

SYZ_CAPI syz_ErrorCode syz_createFastSineBankGenerator(syz_Handle *out, syz_Handle context,
                                                       struct syz_SineBankConfig *bank_config, void *config,
                                                       void *userdata,
                                                       syz_UserdataFreeCallback *userdata_free_callback);
```

Create a sine bank which evaluates itself by summing sine waves at specific multiples of a fundamental frequency.  See
remarks for specifics on what this means and what the values in the configuration structs should be.

Most applications will want to use the helpers which configure the bank with specific well-known waveforms.

You own the memory pointed to by `syz_SineBankConfig`, and it may be freed immediately after the constructor call.
Pointing it at values on the stack is fine.

### Specific waveform helpers

```
SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSine(syz_Handle *out, syz_Handle context,
                                                           double initial_frequency, void *config, void *userdata,
                                                           syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorTriangle(syz_Handle *out, syz_Handle context,
                                                               double initial_frequency, unsigned int partials,
                                                               void *config, void *userdata,
                                                               syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSquare(syz_Handle *out, syz_Handle context,
                                                             double initial_frequency, unsigned int partials,
                                                             void *config, void *userdata,
                                                             syz_UserdataFreeCallback *userdata_free_callback);
SYZ_CAPI syz_ErrorCode syz_createFastSineBankGeneratorSaw(syz_Handle *out, syz_Handle context, double initial_frequency,
                                                          unsigned int partials, void *config, void *userdata,
                                                          syz_UserdataFreeCallback *userdata_free_callback);
```

Create waveforms of specific types, e.g. what you'd get from a digital synthesizer.  Most applications will wish to use
these functions. See remarks for additional notes on quality.

## Properties

Enum | Type | Default Value | Range | Description
--- | --- | --- | --- | ---
`SYZ_P_FREQUENCY` | double | set by constructor | any positive value | the frequency of the waveform.

## Linger behavior

Fades out over a few ms.

## Remarks

This implements a fast sine wave generation algorithm which is on the order of single-digit clock cycles per sample, at
the cost of slight accuracy.  The intended use is to generate chiptune-quality waveforms.  For those not familiar, most
waveforms may be constructed of summed sine waves at specific frequencies, so this functions as a general-purpose wave
generator.  Note that attempts to use this to run a fourier transform will not end well: the slight inaccuracy combined
with being `O(samples*waves)` will cause wave counts over a few hundred to rapidly become impractically slow and low
quality.  In the best case (right processor, your compiler likes Synthizer, etc), the theoretical execution time per
sample for 32 waves is around 5-10 clock cycles, so it can be push pretty far.

In order to specify the waveforms to use, you must use 3 parameters:

- `frequency_mul`: the multiplier on the base frequency for this wave.  For example `frequency_mul = 1.0` is a sine wave
  which plays back at whatever frequency the generator is set to, `2.0` is the first harmonic, etc.  Fractional values
  are permitted.
- `phase`: the phase of the sinusoid in the range 0 to 1.  This is slightly odd because there are enough approximations
  of `PI` out there depending on language; to get to what Synthizer wants, take your phase in radians and divide by
  whatever approximation of `PI` you're using.
- `gain`: Self-explanatory.  Negative gains are valid in order to allow converting from mathematical formulas that use
  it.

In order to provide a more convenient interface, the helper functions for various waveform types may be used. These
specify the number of partials to generate, which does not exactly equate to harmonics because not all waveforms contain
every harmonic.  Simply playing with this value until it sounds good is the easiest way to deal with it; for most
applications, no more than 30 should be required.  More specifically, the square wave makes a good concrete example of
how partials can be different from harmonics because it only includes odd harmonics.  So:

partials | harmonics included
--- | ---
1 | 1.0 (fundamental), 3.0
2 | 1.0 (fundamental), 3.0, 5.0
3 | 1.0, 3.0, 5.0, 7.0

The reason that you might wish to use less partials is due to aliasing.  Extremely high partial counts will alias if
they go above nyquist, currently 22050 hZ.  If you are playing high frequencies lowering the partial count may be called
for.  By contrast, intentionally forcing it to alias can produce more "chiptune"-quality sound.  The CPU usage of more
partials should be unnoticeable for all practical values; if this turns out not to be the case you are encouraged to
open an issue.

This generator does not allow introducing a DC term.  If you need one for some reason, open an issue instead of trying
to hack it in with a sine wave at 0HZ and the appropriate phase and gain.
