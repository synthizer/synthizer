# Decoding Audio Data

## The Quick Overview


Synthizer supports mp3, wav, and flac.  If you need more formats, then you can
[load Libsndfile](./libsndfile.md) or decode the data yourself.

If you need to read from a file, use e.g. `syz_createBufferFromFile`.  If you
need to read from memory, use e.g. `syz_createBufferFromEncodedData`.  If you
need to just shove floats at Synthizer, use `syz_creatBufferFromFloatArray`.

[StreamingGenerator](../object_reference/streaming_generator.md) has a similar
set of methods.  In general you can find out what methods are available in the
object reference.  Everything supports some function that's equivalent to
`syz_createBufferFromFile`.

These functions are the most stable interface because they can be easily
supported across incompatible library versions.  If your app can use them, it
should do so.

## Streams

Almost all of these methods wrap and hide something called a stream handle,
which can be created with e.g. `syz_createStreamHandleFromFile`, then used with
e.g. `syz_createBufferFromStreamHandle`.  Bindings expose this to you, usually
with classes or your language's equivalent (e.g. in Python this is
`StreamHandle`).  This is used to get data from custom sources, for example the
network or encrypted asset stores.  For info on writing your own streams, see
[the dedicated section](./custom_streams.md).

In addition to get streams via specific methods, Synthizer also exposes a
generic interface:

```
SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromStreamParams(syz_Handle *out, const char *protocol, const char *path, void *param);
```

Using the generic interface, streams are referred to with:

- A protocol, for example`"file"`, which specifies the kind of stream it is.
  Users can register their own protocols.
- A path, for example to a file on disk.  This is protocol-specific.
- And a `void *` param, which is passed through to the underlying stream
  implementation, and currently ignored by Synthizer.

So, for example, you might get a file by:

```
syz_createStreamHandleFromStreamParams("file", path, NULL);
```

Streams don't support raw data.  They're always an encoded asset.  So for
example mp3 streams are a thing, but floats at 44100 streams aren't.  Synthizer
will offer a better interface for raw audio data pending there being enough
demand and a reason to go beyond `syz_createBufferFromFloatArray`.
