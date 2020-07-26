# StreamingGenerator

Inherits from [Generator](./generator.md).

## Constructors

### `syz_createStreamingGenerator`

```
SYZ_CAPI syz_ErrorCode syz_createStreamingGenerator(syz_Handle *out, syz_Handle context, const char *protocol, const char *path, const char *options);
```

Creates a StreamingGenerator from the standard [stream parameters](../concepts/streams.md).

## Properties

None currently. Some in future.

## Remarks

StreamingGenerator plays streams, decoding and reading on demand.  Note  that this is a heavyweight object with a long start-up time.  The typical use case is for music playback.

Due to the expense of streaming from disk and other I/O sources, having more than a few StreamingGenerators going will cause a decrease in audio quality on many systems, typically manifesting as drop--outs and crackling.

StreamingGenerator creates 1 background thread per instance.
