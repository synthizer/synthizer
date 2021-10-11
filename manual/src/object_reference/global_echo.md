# GlobalEcho

## Constructors

### `syz_createGlobalEcho`

```
SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context, void *config, void *userdata,
                                            syz_UserdataFreeCallback *userdata_free_callback);
```

Creates an echoe effect.

## Functions

### `syz_echoSetTaps`

```
struct syz_EchoTapConfig {
    double delay;
    double gain_l;
    double gain_r;
};

SYZ_CAPI syz_ErrorCode syz_globalEchoSetTaps(syz_Handle handle, unsigned int n_taps, struct syz_EchoTapConfig *taps);
```

Configure the taps for this Echo.  Currently, delay must be no greater than 5
seconds.  To clear the taps, set the echo to an array of 0 elements.

## Properties

None

## Linger Behavior

Lingers until the delay line is empty, that is until no more echoes can possibly
be heard.

## Remarks

This is a stereo tapped delay line, with a one-block crossfade when taps are
reconfigured.  The max delay is currently fixed at 5 seconds, but this will be
made user configurable in future.

This implementation offers precise control over the placement of taps, at the
cost of not being able to have indefinitely long echo effects.  It's most useful
for modeling discrete, panned echo taps.  Some ways this is useful are:

- Emphasize footsteps off walls in large spaces, by computing the parameters for
  the taps off level geometry.
- Emphasize openings or cooridors.
- Pair it with a reverb implementation to offer additional, highly controlled
  early reflection emphasis

This is effectively discrete convolution for 2 channels, implemented using an
algorithm designed for sparse taps. In other words, the cost of any echo effect
is `O(taps)` per sample.  Anything up to a few thousand discrete taps is
probably fine, but beyond that the cost will become prohibitive.
