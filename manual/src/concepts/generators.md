# Introduction to Generators

generators are how audio first enters Synthizer.  They can do things like [play
a buffer](../object_reference/buffer_generator.md), [generate
noise](../object_reference/noise_generator.md), or [stream audio
data](../object_reference/streaming_generator.md).  By themselves, they're
silent and don't do anything, so they must be [connected to
sources](./sources.md) via `syz_sourceAddGenerator`.

Generators are like a stereo without speakers: you have to plug them into
something else before they're audible.  In this case the "something else" is a
source.  Synthizer only supports using a generator with one source at a time,
but every source can have multiple generators.  That is, given generators `g1`
and `g2` and sources `s1` and `s2`, then `g1` and `g2` could be conneccted to
`s1`, `g1` to `s1` and `g2` to `s2`, but not `g1` to both `s1` and `s2` at the
same time.