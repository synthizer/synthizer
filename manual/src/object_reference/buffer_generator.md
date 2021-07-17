# BufferGenerator

## Constructors

### `syz_createBufferGenerator`

```
SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context, void *userdata, syz_UserdataFreeCallback *userdata_free_callback);
```

Creates a BufferGenerator. The buffer is set to NULL and the resulting generator
will play silence until one is associated.

## Properties

Enum | Type | Default Value | Range | Description
--- | --- | --- | --- | ---
SYZ_P_BUFFER | Object | 0 | Any Buffer handle | The buffer to play
SYZ_P_PLAYBACK_POSITION | double | 0.0 | value >= 0.0 | The position in the buffer.
SYZ_P_LOOPING | int | 0 | 0 or 1 | Whether playback loops at the end of the buffer.

## Linger behavior

Disables looping and plays until the buffer ends.

## Remarks

BufferGenerators play [Buffer](./buffer.md)s.  This is the most efficient way to
play audio.

`SYZ_P_PLAYBACK_POSITION` is reset if `SYZ_P_BUFFER` is modified.

`SYZ_P_PLAYBACK_POSITION` can be set past the end of the buffer.  If
`SYZ_P_LOOPING = 0`, the generator will play silence.  Otherwise, the position
will immediately loop to the beginning.

More than one BufferGenerator can use the same underlying Buffer.

If the buffer being used by this generator is destroyed, this generator
immediately begins playing silence until another buffer is associated.
