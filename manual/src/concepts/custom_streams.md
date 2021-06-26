# Implementing Custom Streams and Custom Stream Protocols

Synthizer supports implementing custom streams in order to read from places that
aren't files or memory: encrypted asset stores, the network, and so on.  This
section explains how to implement them.

before continuing, carefully consider whether you need this.  Implementing a
stream ina  higher level language and forcing Synthizer to go through it has a
small but likely noticeable performance hit.  It'll work fine, but the built-in
functionality will certainly be faster and more scalable.  Implementing a stream
in C is a complex process.  If your app can use the already-existing
funtionality, it is encouraged to do so.

## A Complete Python Example

The rest of this section will explain ind detail how streams work from the C
API, but this is a very complex topic and most of the infrastructure which
exists for it exists to make it possible to write convenient bindings.
Consequently, here is a complete and simple custom stream which wraps a Python
file object, registered as a custom protocol:

```
class CustomStream:
    def __init__(self, path):
        self.file = open(path, "rb")

    def read(self, size):
        return self.file.read(size)

    def seek(self, position):
        self.file.seek(position)

    def close(self):
        self.file.close()

    def get_length(self):
        pos = self.file.tell()
        len = self.file.seek(0, 2)
        self.file.seek(pos)
        return len

def factory(protocol, path, param):
    return CustomStream(path)


synthizer.register_stream_protocol("custom", factory)
gen = synthizer.StreamingGenerator.from_stream_params(ctx, "custom", sys.argv[1])
```

Your bindings will document how to do this, for example in Python see
`help(synthizer.register_stream_protocol)`.  it's usually going to be this level
of complexity when doing it from a binding.  The rest of this section explains
what's going on from the C perspective, but non-C users are still encouriaged to
read it because it explains the general idea and offers best practices for
efficient and stable stream usage.

It's important to note that though this example demonstrates using
`StreamingGenerator`, buffers have similar methods to decode themselves from
streams.  Since `StreamingGenerator` has a large latency for anything but the
initial start-up, the primary use case is actually likely to be buffers.

## The C Interface

To define a custom stream, the following types are used:

```
typedef int syz_StreamReadCallback(unsigned long long *read, unsigned long long requested, char *destination, void *userdata, const char ** err_msg);
typedef int syz_StreamSeekCallback(unsigned long long pos, void *userdata, const char **err_msg);
typedef int syz_StreamCloseCallback(void *userdata, const char **err_msg);
typedef void syz_StreamDestroyCallback(void *userdata);

struct syz_CustomStreamDef {
    syz_StreamReadCallback *read_cb;
    syz_StreamSeekCallback *seek_cb;
    syz_StreamCloseCallback *close_cb;
    syz_StreamDestroyCallback *destroy_cb;
    long long length;
    void *userdata;
};

SYZ_CAPI syz_ErrorCode syz_createStreamHandleFromCustomStream(syz_Handle *out, struct syz_CustomStreamDef *callbacks);

typedef int syz_StreamOpenCallback(struct syz_CustomStreamDef *callbacks, const char *protocol, const char *path, void *param, void *userdata, const char **err_msg);
SYZ_CAPI syz_ErrorCode syz_registerStreamProtocol(const char *protocol, syz_StreamOpenCallback *callback, void *userdata);
```

The following sections explain how these functions work.

## Ways To Get A Custom Stream

There are two ways to get a custom stream.  You can:

- Fill out the callbacks in `syz_CustomStreamDef` and use
  `syz_createStreamHandleFromCustomStream`.
- Write a function which will fill out `syz_CustomStreamDef` from the standard
  stream parameters, and register a protocol with `syz_registerStreamProtocol`.

The difference between these is the scope: if you don't register a protocol,
only your app can access the custom stream, presumably via a module that
produces them.  This is good because it keeps things modular.  If registering a
protocol, however, the protocol can be used from anywhere in the process,
including other libraries and modules.  For example, writing a
`encrypted_sqlite3` protocol C library could then be used to add the
`"encrypted_sqlite3"` protocol to any language.

Protocol names must be unique.  The behavior is undefined if they aren't.  A
good way of ensuring this is to namespace them.  For example,
`"ahicks92.my_super_special_protocol"`.

The `void *param` parameter is reserved for your implementation, and passed to
the factory callback if using the stream parameters approach.  It's assumed that
implementations going through `syz_createStreamHandleFromCustomStreamDef`
already have a way to move this information around.

## Non-callback `syz_CustomStreamDef` parameters

These are:

- `length`, which must be set and known for seekable streams.  If the length of
  the stream is unknown, set it to -1.
- `userdata`, which is passed as the userdata parameter to all stream callbacks.

## The Stream Callbacks

Streams have the following callbacks, with mostly self-explanatory parameters:

- If going through the protocol interface, the open callback is called when the
  stream is first opened.  If going through
  `syz_createStreamHandleFromCustomStream`, it is assumed that the app already
  opened the stream and has put whatever it is going to need into the `userdata`
  field.
- After that, the read and (if present) seek callbacks are called until the
  stream is no longer needed.  The seek callback is optional.
- The close callback is called when Synthizer will no longer use the underlying
  asset.
- The destroy callback, optional, is called when it is safe to free all
  resources the stream is using.

For more information on why we offer both the close and destroy callback, see
below on error handling.

All callbacks should return 0 on success, and (if necessary) write to their out
parameters.

The read callback must always read exactly as many bytes are requested, never
more.  If it reads less bytes than were requested, Synthizer treats this as an
end-of-stream condition.  If the end of the stream has already been reached, the
read callback should claim it read no bytes.

The seek callback is optional.  Streams don't need to support seeking, but (1)
this disables seeking in `StreamingGenerator` and (2) this disables support for
Libsndfile if Libsndfile was loaded.  In order to be seekable, a stream must:

- Have a seek callback; and
- Fill out the `length` field with a positive value, the length of the stream in
  bytes.

## Error Handling

To indicate an error, callbacks should return a non-zero return value and
(optionally) set their `err_msg` parameter to a string representation of the
error.  Synthizer will log these errors if logging is enabled.  For more complex
error handling, apps are encouraged to ferry the information from streams to
their main threads themselves.  If a stream callback fails, Synthizer will
generally stop the stream all together.  Consequiently, apps should do their
best to recover and never fail the stream.  Synthizer takes the approach of
assuming that any error is likely unrecoverable and expects that implementations
already did their best to succeed.

if the read callback fails, the position of the stream isn't updated.  If the
seek callback fails, Synthizer assumes that the position didn't move.

the reason that Synthizer offers a destroy callback in addition to one for
closing is so that streams may use non-static strings as error messages.
Synthizer may not be done logging these when the stream is closed, so apps doing
this should make sure that they live at least as long as the destroy callback,
after which Synthizer promises to never use anything related to this stream
again.

The simplest way to handle error messages for C users is to just use string
constants, but for other languages such as Python it is useful to be able to
convert errors to strings and attach them to the binding's object so that these
can be logged.  The destroy callback primarily exists for this use case.

Synthizer makes one more guarantee on the lifetime required of `err_msg`
strings: they need only live as long as the next time a stream callback is
called.  This means that, for example, the Python binding only keeps the most
recent error string around and replaces it as necessary.

## Thread Safety

Streams will only ever be used by one thread at a time, but may be moved between
threads.
