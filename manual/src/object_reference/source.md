# Source (abstract)

## Constructors

None.

## Properties

None currently. Probably `SYZ_P_GAIN` moved down from `SpatializedSource` in the near future.

## Functions

### `syz_sourceAddGenerator`, `syz_sourceRemoveGenerator`

```
SYZ_CAPI syz_ErrorCode syz_sourceAddGenerator(syz_Handle source, syz_Handle generator);
SYZ_CAPI syz_ErrorCode syz_sourceRemoveGenerator(syz_Handle source, syz_Handle generator);
```

Add/remove a generator from a source. Each generator may be added once and duplicate add calls will have no effect. Each generator should only be used with one source at a time.


## Remarks

Sources represent audio output.  They combine all generators connected to them, apply any effects if necessary, and feed the context.
