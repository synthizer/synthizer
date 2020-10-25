# Echo

IMPORTANT: this object is provisional and may be subject to change.

## Constructors

### `syz_createGlobalEcho`

```
SYZ_CAPI syz_ErrorCode syz_createGlobalEcho(syz_Handle *out, syz_Handle context);
```

Creates the global variant of the echo effect.

## Functions

### `syz_echoSetTaps`

```
struct EchoTapConfig {
	float delay;
	float gain_l;
	float gain_r;
};

SYZ_CAPI syz_ErrorCode syz_echoSetTaps(syz_Handle handle, unsigned int n_taps, struct EchoTapConfig *taps);
```

Configure the taps for this Echo.  Currently, delay must be no greater than 5 seconds.  To clear the taps, set the echo
to an array of 0 elements.

## Properties

None

## Remarks

This is a stereo tapped delay line, with a one-block crossfade when taps are reconfigured.  The max delay is currently fixed at 5 seconds, but this will be made user configurable in future.

This implementation offers precise control over the placement of taps, at the cost of not being able to have indefinitely long echo effects.  It's most useful for modeling discrete, panned
echo taps.  Some ways this is useful are:

- Emphasize footsteps off walls in large spaces, by computing the parameters for the taps off level geometry.
- Emphasize openings or cooridors.
- Pair it with a reverb implementation to offer additional, highly controlled early reflection emphasis

This is effectively discrete convolution for 2 channels, implemented using an algorithm designed for sparse taps.
In other words, the cost of any echo effect is `O(taps)`.  Anything up to a few thousand discrete taps is probably fine, but beyond that the cost will become prohibitive.
