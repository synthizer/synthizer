# Source (abstract)

## Constructors

None.

## Properties

Enum | Type | Default | Range | Description
--- | --- | --- | --- | ---
SYZ_P_GAIN | double | Any double > 0 | An additional gain factor applied to this source.

## Functions

### `syz_sourceAddGenerator`, `syz_sourceRemoveGenerator`

```
SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);
```

Add/remove a generator from a source. Each generator may be added once and duplicate add calls will have no effect. Each generator should only be used with one source at a time.

## Remarks

Sources represent audio output.  They combine all generators connected to them, apply any effects if necessary, and feed the context. Subclasses of Source add panning and other features.
