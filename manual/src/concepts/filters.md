# Filters

Synthizer supports a filter property type, as well as filters on effect sends.  The API for this is as follows:

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

The struct `syz_BiquadConfig` is an opaque struct whose fields are only exposed to allow allocating them on the stack.  It represents
configuration for a biquad filter, designed using the [Audio EQ Cookbook](../appendices/audio_eq_cookbook.md).
It's initialized with one of the above design functions.

A suggested default for `q` is `0.7071135624381276`, which gives Buttererworth lowpass and highpass filters.
For those not already familiar with biquad filters, `q` controls resonance: higher values of `q` will cause the filter to ring for some period of time.

In future, Synthizer will stabilize the `syz_BiquadConfig` struct and use it to expose more options, e.g. automated filter modulation.
