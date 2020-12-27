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

### `syz_pause`, `syz_play`

```
syz_ErrorCode syz_pause(syz_Handle object);
syz_ErrorCode syz_play(syz_Handle object);

```

The standard play/pause functions.  Note that all subclasses of Source will still process panners for the time being,
so this doesn't make a Source free in terms of things like HRTF and effect sends.  This case will be optimized further in future.

When a Source is paused, no generator connected to it advances even if the generator is unpaused.

## Remarks

Sources represent audio output.  They combine all generators connected to them, apply any effects if necessary, and feed the context. Subclasses of Source add panning and other features.
