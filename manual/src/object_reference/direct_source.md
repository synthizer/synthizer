# DirectSource

## Constructors

### `syz_createDirectSource`

```
SYZ_CAPI syz_ErrorCode syz_createDirectSource(syz_Handle *out, syz_Handle context);
```

Creates a direct source.

## Properties

Inherited from Source only.

## Remarks

A direct source is for music and other audio assets that don't wish to participate in panning, , and should be linked directly to speakers.

Audio is converted to the Context's channel count and passed directly through.