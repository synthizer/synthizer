# SpatializedSource (abstract)

## Constructors

None

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | any finite double | An additional gain factor applied to this source in DB
SYZ_P_PANNER_STRATEGY | int | SYZ_PANNER_STRATEGY_HRTF | any SYZ_PANNER_STRATEGY | The panner strategy for this source.

## Remarks

SpatializedSource is an abstract class which gives all panned sources their distance model and gain.
