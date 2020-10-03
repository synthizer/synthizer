# BufferGenerator

Inherits from [Generator](./generator.md).

## Constructors

### `syz_createBufferGenerator`

```
SYZ_CAPI syz_ErrorCode syz_createBufferGenerator(syz_Handle *out, syz_Handle context);
```

Creates a BufferGenerator. The buffer is set to NULL and the resulting generator will play silence until one is associated.

## Properties

Enum | Type | Default Value | Range | Description
--- | --- | --- | --- | ---
SYZ_P_BUFFER | Object | 0 | Any Buffer handle | The buffer to play
SYZ_P_POSITION | double | 0.0 | value >= 0.0 | The position in the buffer.
SYZ_P_LOOPING | int | 0 | 0 or 1 | Whether playback loops at the end of the buffer.
SYZ_P_PITCH_BEND | double | 1.0 | value >= 0.0 | Multiplier on the rate of the buffer's playback.

## Remarks

BufferGenerators play [Buffer](./buffer.md)s.

`SYZ_P_POSITION` is reset if `SYZ_P_BUFFER` is modified.

`SYZ_P_POSITION` can be set past the end of the buffer.  If `SYZ_P_LOOPING = 0`, the generator will play silence.  Otherwise, the position will immediately loop to the beginning.

More than one BufferGenerator can use the same underlying Buffer.


`SYZ_P_PITCH_BEND` is a multiplicative rate on the playback of the buffer.  Though this clarification is currently unimportant, the difference between this and it being *the* rate
is that it will eventually be combined with doppler effects.  A value of 2.0 is one octave higher. A value of 0.5 is one octave lower.  Expect `SYZ_P_PITCH_BEND` to move to a base "class" in future:
though we only support it for buffers at the moment, many other generator types will be able to do so in future.
