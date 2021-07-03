# Operations Common to Panned and 3D Sources

## Constructors

None

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_PANNER_STRATEGY | int | delegated to Context | any SYZ_PANNER_STRATEGY | The panner strategy for this source.

## Remarks

All panned sources allow setting their panner strategy, which is taken by
default from `SYZ_P_DEFAULT_PANNER_STRATEGY` on the context. See [3D
Audio](../concepts/3d_audio.md) for info on panner strategies.
