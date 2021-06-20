#  3D Panning 

This page explains the steps involved in 3D panning. Note that only the panner strategy applies to `PannedSource`.

## 1. Convert the source's position from world coordinates to Azimuth, Elevation, and distance

This is done by converting the position, at, and up vectors to a transformation matrix.  The result is a position in listener coordinates. Then application of the pythagorean theorem and basic trigonometry gets to spherical coordinates.

## 2. Compute the Gain from the Distance Model

Let `d` be the distance to the source, `d_ref` the reference distance, `d_max` the max distance, `r` the roll-off factor. Then the gain of the source is computed as a linear scalar using one of the following formulas:

Model | Formula
--- | ---
SYZ_DISTANCE_MODEL_NONE | `1.0`
SYZ_DISTANCE_MODEL_LINEAR | 1 - r * (clamp(d, d_ref, d_max) - d_ref) / (d_max - d_ref);
SYZ_DISTANCE_MODEL_EXPONENTIAL when `d_ref == 0.0` | 0.0
SYZ_DISTANCE_MODEL_EXPONENTIAL when `d_ref > 0.0` | `(max(d_ref, d) / d_ref) ** -r`
SYZ_DISTANCE_MODEL_INVERSE when `d_ref = 0.0` | `0.0`
SYZ_DISTANCE_MODEL_INVERSE when `d_ref > 0.0` | `d_ref / (d_ref + r * max(d, d_ref) - d_ref)`

Qualitatively, `d_ref` is the "size" of the source, `d_max` is where the source is silent, `r` is how fast the source becomes quieter.  Mapping these to real world scenarios is difficult, and in general the best approach is to experiment for your use case.

## 3. Apply the closeness boost and clamp

The closeness boost, specified through `SYZ_P_CLOSENESS_BOOST` and `SYZ_P_CLOSENESS_BOOST_DISTANCE` is used to emphasize sources that have crossed a threshold of interest, i.e. because the player is now close enough to interact.
`SYZ_P_CLOSENESS_BOOST` specifies a gain in DB (negative DB is allowed) which is added to the source's gain when the source is closer than `SYZ_P_CLOSENESS_BOOST_DISTANCE`.

After the closeness boost is applied, gain is clamped to the range 0.0 to 1.0.

## 4. Apply the Panning Strategy

The panning strategy specifies how sources are to be panned.  SYnthizer supports the following panning strategies:

Strategy | Channels | Description
--- | --- | ---
SYZ_PANNER_STRATEGY_HRTF | 2 | An HRTF implementation, intended for use via headphones.
SYZ_PANNER_STRATEGY_STEREO | 2 | A simple stereo panning strategy assuming speakers are at -90 and 90.
