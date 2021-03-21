# FdnReverb

A reverb based off a feedback delay network.

Inherits from [GlobalEffect](./global_effect.md).

This is provisional functionality, and subject to change.

## Constructors

### `syz_createGlobalFdnReverb`

```
SYZ_CAPI syz_ErrorCode syz_createGlobalFdnReverb(syz_Handle *out, syz_Handle context);
```

Creates a global FDN reverb with default settings.

## Properties

See remarks for a description of what these do and how to use them effectively.

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_MEAN_FREE_PATH | double | 0.02 | 0.0 to 0.5 | The mean free path of the simulated environment.
SYZ_P_T60 | double | 1.0 | 0.0 to 100.0 | The T60 of the reverb
SYZ_P_LATE_REFLECTIONS_LF_ROLLOFF | double | 1.0 | 0.0 to 2.0 | A multiplicative factor on T60 for the low frequency band
SYZ_P_LATE_REFLECTIONS_LF_REFERENCE | double | 200.0 | 0.0 to 22050.0 | Where the low band of the feedback equalizer ends
SYZ_P_LATE_REFLECTIONS_HF_ROLLOFF | double | 0.5 | 0.0 to 2.0 | A multiplicative factor on T60 for the high frequency band
SYZ_P_LATE_REFLECTIONS_HF_REFERENCE | double | 500.0 | 0.0 to 22050.0 | Where the high band of the equalizer starts.
SYZ_P_LATE_REFLECTIONS_DIFFUSION | double | 1.0 | 0.0 to 1.0 | Controls the diffusion of the late reflections as a percent.
SYZ_P_LATE_REFLECTIONS_MODULATION_DEPTH | double | 0.01 | 0.0 to 0.3 | The depth of the modulation of the delay lines on the feedback path in seconds.
SYZ_P_LATE_REFLECTIONS_MODULATION_FREQUENCY | double | 0.5 | 0.01 to 100.0 | The frequency of the modulation of the delay lines inthe feedback paths.
SYZ_P_LATE_REFLECTIONS_DELAY | double | 0.01 | 0.0 to 0.5 | The delay of the late reflections relative to the input in seconds.

Note that `SYZ_P_INPUT_FILTER` defaults to a lowpass Butterworth with a cutoff frequency of 1500 HZ.

## Remarks

This is a reverb composed of a feedback delay network with 8 internal delay lines.  The algorithm proceeds as follows:

- Audio is fed through the input filter, a lowpass.  Use this to eliminate high frequencies, which can be quite harsh when fed to reverb algorithms.
- Then, audio is fed into a series of 8 delay lines, connected with a feedback matrix.  It's essentially a set of parallel allpass filters with some additional feedbacks, but inspired by physics.
  - Each of these delay lines is modulated, to reduce periodicity.
  - On each feedback path, the audio is fed through an equalizer to precisely control the decay rate in 3 frequency bands.
- Two decorrelated channels are extracted. This will be increased to 4 when surround sound support is added.
- Finally, the output is delayed by the late reflections delay.

The current reverb modle is missing spatialized early reflections.  Practically speaking this makes very little difference when using an FDN because the FDN simulates them effectively on its own, but the
`SYZ_P_EARLY_REFLECTIONS_*` namespace is reserved for that purpose.  The plan is to feed them through HRTF in order to attempt to capture the shape of the room, possibly with a per-source model.

The reverb is also missing the ability to pan late reflections; this is on the roadmap.

The default configuration is something to the effect of a medium-sized room.  Presets will be added in future.  The following sections explain considerations for reverb design with this algorithm:

### A Note On Property Changes

The FdnReverb effect involves a large amount of feedback and is therefore impossible to crossfade efficiently. To that end,we don't try.  Expect most property changes save for t60 and the hf/lf frequency controls to cause clicking and other artifacts.

To change properties smoothly, it's best to create a reverb, set all the parameters, connect all the sources to the new one, and disconnect
all the sources from the old one, in that order.  Synthizer may eventually do this internally, but that necessitates taking a permanent and large allocation cost without a lot of implementation work being done first, so for the moment we don't.

In practice, this doesn't matter.  Most environments don't change reverb characteristics.  A good flow is as follows:

- Design the reverb in your level editor/other environment.
- When necessary, use `syz_effectReset` for interactive experimentation.
- When distributing/launching for real, use the above crossfading instructions.

It is of course possible to use more than one reverb at a time as well, and to fade sources between them at different levels. Note, however, that reverbs are relatively expensive.

### The Input Filter

Most reverb algorithms have a problem: high frequencies are emphasized.  Synthizer's is no different.  To solve this, we introduce an input lowpass filter, which can cut out the higher frequencies.
This is `SYZ_P_FILTER_INPUT`, available on all effects, but defaulted by the reverb to a lowpass at 1500 HZ because most of the negative
characteristics of reverbs occur when high frequencies are overemphasized.

Changing this cutoff filter is the strongest tool available for coloring the reverb.  Low cutoffs are great for rooms with sound dampening, high cutoffs for concrete walls.
It can be disabled, but doing so will typically cause metallic and periodic artifacts to be noticeable.

It's also possible to swap it with other filter types.  Lowpass filters are effectively the only filter type that aligns with the real world
in the context of a reverb, but other filter types can produce interesting effects.

### Choosing the mean free path and late reflections delay

These two values are most directly responsible for controlling how big a space feels.  Intuitively, the mean free path is the average distance from wall to wall, and the late reflections delay
is the time it takes for audio to hit something for the first time.  In general, get the mean free path by dividing the average distance between the walls by the speed of sound, and set the late reflections delay to something in the same order of magnitude.

A good approximation for the mean free path is `4 * volume / surface_area`.  Mathematically, it's the average time sound travels before reflection off an obstacle.  Very large mean free paths produce many discrete echoes.  For unrealistically large values, the late reflections won't
be able to converge at all.

### Choosing T60 and controlling per-band decay

The t60 and related properties control the gains and configuration of a filter on the feedback path.

The t60 of a reverb is defined as the time it takes for the reverb to decay by `-60db`.  Effectively this can be thought of as how long until the reverb is completely silent.
0.2 to 0.5 is a particularly reverberant and large living room, 1.0 to 2.0 is a concert hall, 5.0 is an amazingly large cavern, and values larger than that quickly become unrealistic and metallic.

Most environments don't have the same decay time for all frequency bands, so the FdnReverb actually uses a 3-band equalizer instead of raw gains on the feedback paths.  The bands are as follows:

- 0.0 to `SYZ_P_LATE_REFLECTIONS_LF_REFERENCE`
- `SYZ_P_LATE_REFLECTIONS_LF_REFERENCE` to `SYZ_P_LATE_REFLECTIONS_HF_REFERENCE`
- `SYZ_P_LATE_REFLECTIONS_HF_REFERENCE` to nyquist

`SYZ_P_T60` controls the decay time of the middle frequency band.  The lower band is `t60 * lf_rolloff`, and the upper `t60 * hf_rolloff`.  This allows you to simply change T60, and let the rolloff ratios
control coloration.

Intuitively, rooms with carpet on all the walls have a rather low hf reference and rolloff, and giant stone caverns are close to equal in all frequency bands. The lf reference/rolloff pairing can be used primarily for
non-natural base boosting.  When the reverb starts, all frequencies are relatively equal but, as the audio continually gets fed back through the feedback paths,
the equalizer will emphasize or deemphasize the 3 frequency bands at different rates.  To use this effectively, treat the hf/lf as defining the materials of the wall, then move t60.

Note that the amount of coloration you can get from the equalizer is limited especially for short reverbs.  To control the perception of the environment more bluntly and independently of t60, use the input filter.

### Diffusion

The diffusion of the reverb is how fast the reverb tail transitions from discrete echoes to a continuous reverberant response.  Synthizer exposes this to you as a percent-based control, since it's not conveniently possible to tie
anything to a real physical quantity in this case.  Typically, diffusion at 1.0 (the default) is what you want.

Another way to think of diffusion is how rough the walls are, how many obstacles there are for sound to bounce off of, etc.

### Delay Line modulation

A problem with feedback delay networks and/or other allpass/comb filter reverb designs is that they tend to be obviously periodic.  To deal with this, modulation of the delay
lines on the feedback path is often introduced.  The final stage of designing an FdnReverb is to decide on the values of the modulation depth and frequency.

The trade-off here is this:

- At low modulation depth/frequency, the reverb likes to sound metallic.
- At high modulation depth/frequency, the reverb gains very obvious nonlinear effects.
- At very high modulation depth/frequency, the reverb doesn't sound like a reverb at all.

FdnReverb tries to default to universally applicable settings, but it might still be worth adjusting these. To disable modulation all together, set the depth to 0.0; due to internal details, setting the frequency to 0.0 is not possible.

The artifacts introduced by large modulation depth/frequency values are least noticeable with percussive sounds and most noticeable with constant tones such as pianos and vocals.  Inversely, the periodic artifacts of no or little modulation
are most noticeable with percussive sounds and least  noticeable with constant tones.

In general, the best way to not need to touch these settings is to use realistic t60, as the beginning of the reverb isn't generally periodic.
