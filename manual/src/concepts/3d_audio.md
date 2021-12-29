#  3D Audio, Panning, and HRTF

## Introduction

Synthizer supports panning audio through two interfaces.

First is [AngularPannedSource and ScalarPannedSource](../object_reference/panned_sources.md), which provides
simple azimuth/elevation controls and the ability to pan based off a scalar, a
value between -1 (all left) and 1 (all right).  In this case the user
application must compute these values itself.

The second way is to use [Source3D](../object_reference/source_3d.md), which
simulates a 3D environment when fed positional data.  This ection concerns
itself with proper use of `Source3D`, which is less straightforward for those
who haven't had prior exposure to these concepts.

## Introduction

There are two mandatory steps to using `Source3D` as well as a few optional
ones.  The two mandatory steps are these:

- On the context, update `SYZ_P_POSITION` and `SYZ_P_ORIENTATION` with the
  listener's position and orientation
- On the source, update `SYZ_P_POSITION` with the source's position.

And optionally:

- Configure the default distance model to control how rapidly sources become
  quiet.
- Emphasize that sources have become close to the listener with the focus boost.
- Add effects (covered in [a dedicated section](./filters_and_effects.md)).

## Don't Move Sources Through the Head

People frequently pick up Synthizer, then try to move the source through the
center of the listener's head, then ask why it's weird and didn't work.  It is
important to realize that this is a physical simulation of reality, and that the
reason you can move the source through the listener's head in the first place is
that this isn't an easily detectable case.  If you aren't driving Synthizer in a
way connected to physical reality--for example if you are attempting to use `x`
as a way to pan sources from left to right and not linked to a position--then
you probably want one of the raw panned sources instead.

## Setting Global Defaults

In addition to controlling the listener, the context offers the ability to set
defaults for all values discussed below.  This is done through a set of
`SYZ_P_DEFAULT_*` properties which match the names of those on sources.  This is
of particular interest to those wishing to use HRTF, which is off by default.

## Synthizer's Coordinate System and Orientations

The short version for those who are familiar with libraries that need position
and orientation data: Synthizer uses a right-handed coordinate system and
configures orientation through [double6 properties](./properties.md) so that it
is possible to atomically set all 6 values at once.  This is represented as a
packed at and up vector pair in the format `(at_x, at_y, at_z, up_x, up_y,
up_z)` on the context under the `SYZ_P_ORIENTATION` property.  As with every
other library doing a similar thing, these are unit vectors.

The short version for those who want a 2D coordinate system where positive y is
north, positive x is east, and player orientations are represented as degrees
clockwise of north: use only x and y of `SYZ_P_POSITION`, and set
`SYZ_P_ORIENTATION` as follows:

```
(sin(angle * PI / 180), cos(angle * PI / 180), 0, 0, 0, 1)
```

The longer version is as follows.

Listener positions are represented through 3 values: the position, the at
vector, and the up vector.  The position is self-explanatory.  The at vector
points in the direction the listener is looking at all times, and the up vector
points out the top of the listener's head, as if there were a pole up the spine.
By driving these 3 values, it is possible to represent any position and
orientation a listener might assume.

Synthizer uses a right-handed coordinate system, which means that if the at
vector is pointed at positive x and the up vector at positive y, positive z
moves sources to the right.  This is called a right-handed coordinate system
because of the right-hand rule: if you point your fingers along positive x and
curl them toward positive y, your finger points at positive z.  This isn't a
standard.  Every library that does something with object positions tends to
choose a different value.  If combining Synthizer with other non-2D components,
it may be necessary to convert between coordinate systems. Resources on how to
do this may easily be found through Google.

The at and up vectors must always be orthogonal, that is forming a right angle
with each other.  In order to facilitate this, Synthizer uses double6 properties
so that both values can and must be set at the same time.  If we didn't, then
there would be a brief period where one was set and the other wasn't, in which
case they would temporarily be invalid.  Synthizer doesn't try to validate that
these vectors are orthogonal and generally tries to do its best when they
aren't, but nonetheless behavior in this case is undefined.

Finally, the at and up vectors must be unit vectors: vectors of length 1.

## Panning strategies and HRTF.

The panning strategy specifies how sources are to be panned.  Synthizer supports
the following panning strategies:

Strategy | Channels | Description
--- | --- | ---
SYZ_PANNER_STRATEGY_HRTF | 2 | An HRTF implementation, intended for use via headphones.
SYZ_PANNER_STRATEGY_STEREO | 2 | A simple stereo panning strategy assuming speakers are at -90 and 90.

When  a source is created, the panning strategy it is to use is passed via the constructor function and cannot be
changed.  A special value, `SYZ_PANNER_STRATEGY_DELEGATE` allows the source to delegate this to the context, and can be
used in cases where the context's configuration should be preferred.  A vast majority of applications will do this
configuration via the context and `SYZ_PANNER_STRATEGY_DELEGATE`; other values should be safed for cases in which you
wish to mix panning types.

By default Synthizer is configured to use a stereo panning strategy, which
simply pans between two speakers.  This is because stereo panning strategies
work on all devices from headphones to 5.1 surround sound systems, and it is not
possible for Synthizer to reliably determine if the user is using headphones or
not.  HRTF provides a much better experience for headphone users but must be
enabled by your application through setting the default panner strategy or doing
so on individual sources.

Since panning strategies are per source, it is possible to have sources using
different panning strategies.  This is useful for two reasons: HRTF is expensive
enough that you may wish to disable it if dealing with hundreds or thousands of
sources, and it is sometimes useful to let UI elements use a different panning
strategy.  An example of this latter case is an audio gauge which pans from left
to right.

## Distance Models

The distance model controls how quickly sources become quiet as they move away
from the listener.  This is controlled through the following properties:

- `SYZ_P_DISTANCE_MODEL`: which of the distance model formulas to use.
- `SYZ_P_DISTANCE_MAX`: the maximum distance at which the source will be
  audible.
- `SYZ_P_DISTANCE_REF`: if you assume your source is a sphere, what's the radius
  of it?
- `SYZ_P_ROLLOFF`: with some formulas, how rapidly does the sound get quieter?
  Generally, configuring this to a higher value makes the sound drop off more
  immediately near the head, then have more subtle changes at further distances.

It is not possible to provide generally applicable advice for what you should
set the distance model to.  A game using meters needs very different settings
than one using feet or light years.  Furthermore, these don't have concrete
physical correspondances.  Of the things Synthizer offers, this is possibly the
least physically motivated and the most artistic from a game design perspective.
In other words: play with different values and see what you like.

The concrete formulas for the distance models are as follows.  Let `d` be the
distance to the source, `d_ref` the reference distance, `d_max` the max
distance, `r` the roll-off factor. Then the gain of the source is computed as a
linear scalar using one of the following formulas:

Model | Formula
--- | ---
SYZ_DISTANCE_MODEL_NONE | `1.0`
SYZ_DISTANCE_MODEL_LINEAR | 1 - r * (clamp(d, d_ref, d_max) - d_ref) / (d_max - d_ref);
SYZ_DISTANCE_MODEL_EXPONENTIAL when `d_ref == 0.0` | 0.0
SYZ_DISTANCE_MODEL_EXPONENTIAL when `d_ref > 0.0` | `(max(d_ref, d) / d_ref) ** -r`
SYZ_DISTANCE_MODEL_INVERSE when `d_ref = 0.0` | `0.0`
SYZ_DISTANCE_MODEL_INVERSE when `d_ref > 0.0` | `d_ref / (d_ref + r * max(d, d_ref) - d_ref)`

## The Closeness Boost

Sometimes, it is desirable to make sources "pop out" of the background
environment.  For example, if the player approaches an object with which they
can interact, making it noticeably louder as the boundary is crossed can be
useful.  This is of primary interest to audiogame designers, a type of game for
the blind, as it can be used to emphasize features of the environment in
non-realistic but informative ways.

This is controlled through two properties:

- `SYZ_P_CLOSENESS_BOOST`: a value in DB controlling how much louder to make the
  sound.  Negative values are allowed.
- `SYZ_P_CLOSENESS_BOOST_DISTANCE`: when the source is closer than this
  distance, begin applying the closeness boost.

When the source is closer than the configured distance, the normal gain
computation still applies, but an additional factor, the number of DB in the
closeness boost, is added.  This means that it is still possible for players to
know if they are getting closer to the source.

The reason that the closeness boost is specified inDB  is that otherwise it
would require values greater than 1.0, and it is primarily going to be fed from
artists and map developers.  If we discover that this is a problem in future, it
will be patched in a major Synthizer version bump.

Note that closeness boost has not gotten a lot of use yet.  Though we are
unlikely to remove the interface, the internal algorithms backing it might
change.
