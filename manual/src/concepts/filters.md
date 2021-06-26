# Filters

Synthizer supports a filter property type, as well as filters on effect sends.
The API for this is as follows:

```
struct syz_BiquadConfig {
...
};

SYZ_CAPI syz_ErrorCode syz_getBiquad(struct syz_BiquadConfig *filter, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setBiquad(syz_Handle target, int property, const struct syz_BiquadConfig *filter);

SYZ_CAPI syz_ErrorCode syz_biquadDesignIdentity(struct syz_BiquadConfig *filter);
SYZ_CAPI syz_ErrorCode syz_biquadDesignLowpass(struct syz_BiquadConfig *filter, double frequency, double q);
SYZ_CAPI syz_ErrorCode syz_biquadDesignHighpass(struct syz_BiquadConfig *filter, double frequency, double q);
SYZ_CAPI syz_ErrorCode syz_biquadDesignBandpass(struct syz_BiquadConfig *filter, double frequency, double bandwidth);
```

See [properties](./properties.md) for how to set filter properties and
[effects](./effects.md) for how to apply filters to effect sends.

The struct `syz_BiquadConfig` is an opaque struct whose fields are only exposed
to allow allocating them on the stack.  It represents configuration for a biquad
filter, designed using the [Audio EQ
Cookbook](../appendices/audio_eq_cookbook.md). It's initialized with one of the
above design functions.

A suggested default for `q` is `0.7071135624381276`, which gives Buttererworth
lowpass and highpass filters. For those not already familiar with biquad
filters, `q` controls resonance: higher values of `q` will cause the filter to
ring for some period of time.

All sources support filters, which may be installed in 3 places:

- `SYZ_P_FILTER`: applies to all audio traveling through the source.
- `SYZ_P_FILTER_DIRECT`: applied after `SYZ_P_FILTER` to audio going directly to
  the speakers/through panners.
- `SYZ_P_FILTER_EFFECTS`: Applied after `SYZ_P_FILTER` to audio going to
  effects.

This allows filtering the audio to effects separately, for example to cut high
frequencies out of reverb on a source-by-source basis.

Additionally, all effects support a `SYZ_P_FILTER_INPUT`, which applies to all
input audio to the effect.  So, you can either have:

```
source filter -> direct path filter -> speakers
```

Or:

```
source filter -> effects filter outgoing from source -> filter on effect send -> input filter to effect -> effect
```

In future, Synthizer will stabilize the `syz_BiquadConfig` struct and use it to
expose more options, e.g. automated filter modulation.
