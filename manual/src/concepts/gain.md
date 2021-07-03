# Setting Gain/volume

## `SYZ_P_GAIN`

All objects which play audio (generators, sources, contexts) offer a
`SYZ_P_GAIN` property, a double scalar between 0.0 and infinity which controls
object volume.  For example `2.0` is twice the amplitude and `0.5` is half the
amplitude.  This works as you'd expect: if set on a generator it only affects
that generator, if on a source it affects everything connected to the source,
and so on.  If a generator is set to 0.5 and the source that it's on is also
0.5, the output volume of the generator is 0.25 because both gains apply in
order.

This means that it is possible to control the volume of generators relative to
each other when all connected to the same source, then control the overall
volume of the source.

## A Note on Human Perception

Humans don't perceive amplitude changes as you'd expect.  For example, moving
from 1.0 to 2.0 will generally sound like a large gap in volume, but from 2.0 to
3.0 much less so, and so on.  Most audio applications that expose volume sliders
to humans will expose them as decibels and convert to an amplitude factor
internally.  If you're just writing a game, you can mostly ignore this, but if
you're doing something more complicated a proper understanding of decibels is
important.  In decibals, a gain of 1.0 is at 0 dB and every increase and/or
decrease by 1 decibel sounds like the same amount of loudness as any other.  The
specific formulas to get to and/or from a gain are as follows:

```
decibels = 20 * log10(gain)
gain = 10**(db/20)
```

Where `**` is exponentiation.

The obvious question is of course "why not expose this as decibels?"  The
problem with decibels is that gains over 1.0 will clip in most applications. But
a gain of 1.0 in decibels is 0 dB.  If there are two incredibly loud sounds both
with a gain of 1.0 playing at the same time, the overall gain is effectively
2.0, which can clip in the same way.  But 0 dB + 0 dB is still 0 dB even though
the correct gain is 2.0.  This gets worse for gains below 0.  Consider 0.5,
which is equivalent to roughly -6 dB.  0.5 + 0.5 is 1, but -6 + -6 is -12 dB.
Which isn't only wrong, it even moved in the wrong direction all together.

As a consequence Synthizer always uses multiplicative factors on the amplitude,
not decibels.  Unless you know what you're doing, you should convert to gain as
soon as possible and reason about how this works as a multiplier.
