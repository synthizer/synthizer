# Filters in Python

Synthizer offers the ability to set filters in many places, in order to allow
for precise control over audio effects.  By far the most useful of these is on
all sources, which can be used to simulate occlusion.  Some places filters may
be found are:

- All source types, as:
  - A `filter` property, which applies to all audio coming out of the source.
  - A `filter_direct` property, which runs after `filter` but only to audio
    going to the direct path (e.g. not through effects).
  - a `filter_effects` property, which also runs after `filter` but only on
    audio going to effects.
- On all effects, as a `filter_input` property, which filters the input to an
  effect.
- As a `filter` parameter to `Context.config_route`, which will apply to audio
  traveling through that effect send.

There are basically two paths, as to how audio can get filtered.  First is
`filter` followed by `filter_direct`.  Second is `filter`, `filter_effects`, the
filter on the send, then `filter_input` on the effect.  `filter` is on both
paths so that it can be used to control the audio from the source.

practically, occlusion goes on either `filter` or `filter_direct` depending if
you want it fed into reverb, `filter_input` on reverbs provides a per-reverb
coloration of the "walls", and the filter in the effect send can be used to
provide per-source coloration for the effect that it's going to.

Currently, the properties are readonly until such time as Synthizer makes
`struct syz_BiquadConfig` non-opaque.

Synthizer supports lowpass, bandpass, and highpass filters.  You get them as
follows:

```
source.filter = synthizer.BiquadConfig.design_lowpass(frequency, q)
source.filter = synthizer.BiquadConfig.design_highpass(frequency, q)
source.filter = synthizer.BiquadConfig.design_bandpass(frequency, bandwidth)

context.config_route(output, input, filter = synthizer.BiquadConfig.design_lowpass(1000))
```

In the above, `q` is an advanced parameter that defaults to a value which yields
a butterworth filter, which is almost always what you want.  You shouldn't need
to change it from the default, and can usually just omit it.  `q` controls
resonance. higher values of `q` produce filters that ring, which may or may not
be beneficial for designing audio effects.

To clear a filter, set it to `synthizer.BiquadConfig.design_identity()`, which
is how you get the filter which does nothing to the audio (internally Synthizer
will avoid running it, but filters do not have a concept of NULL).

Note that not all filter configurations are stable.  Synthizer cannot validate
this case in any meaningful fashion.  All normal usage should work as expected,
but extreme values may produce unstable filters.  For example: lowpasses with
absurdly high frequencies, bandpasses with a bandwidth of 1 HZ, and/or very low
and very high `q` values. For those not already familiar with unstable filters,
this case can be recognized by strange metallic ringing effects that run
forever, even when audio is silent.

To design occlusion, use a lowpass filter on the source, either as `filter` or
`filter_direct`.  Synthizer doesn't currently provide anything to help because
it's not possible to build a proper physics-based occlusion model and it is
sometimes even beneficial to use bandpass or highpass filters instead (e.g.
haudio traveling through a pipe). It has to be done per application.
