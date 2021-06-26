# The Synthizer C API

Synthizer has the following headers:

- `synthizer.h`: All library functions
- `synthizer_constants.h`: Constants, i.e. the very large property enum.

These headers are separated because it is easier in some cases to machine parse
`synthizer_constants.h` when writing bindings for languages which don't have
good automatic generation, then manually handle `synthizer.h`.

The Synthizer C API returns errors and writes results to out parameters.  Out
parameters are always the first parameters of a function, and errors are always
nonzero.  Note that error codes are currently not defined; they will be, once
things are more stable.

It is possible to get information on the last error using these functions:

```
SYZ_CAPI syz_ErrorCode syz_getLastErrorCode(void);
SYZ_CAPI const char *syz_getLastErrorMessage(void);
```

These are thread-local functions.  The last error message is valid until the
next call into Synthizer.
