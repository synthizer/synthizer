# Objects, Handles, and Properties

Synthizer represents references to objects with a `syz_Handle` type.  The following excerpts from `synthizer.h` are available on every object type:

```
SYZ_CAPI syz_ErrorCode syz_handleFree(syz_Handle handle);

SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value);
SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value);
SYZ_CAPI syz_ErrorCode syz_getO(syz_Handle *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);
SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z);
SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2);
```

Synthizer objects are like classes: they have properties, methods, and (optionally) bases. They're created through "constructors", for example `syz_createContext`, and destroyed through `syz_handleFree`.

Property names are defined in `synthizer_constants.h` of the form `SYZ_P_FOO`.  Since some objects have common properties and in order to preserve flexibility, the property enum is shared between all objects.

Properties are set through `syz_setX` and read through `syz_getX`.  `syz_setX` is fast.  `syz_getX` is slow and should only be used for debugging.  Programs are strongly discouraged from reading properties for any other purpose. See below for more explanation of `syz_getX`.

Synthizer supports 5 property types: int, double, double3, double6, and object.

Double3 is typically used for position and double6 for orientation.  Synthizer's coordinate system is right-handed, configured so that positive y is forward, positive x east, and positive z up.  Listener orientation is controlled through the context.

Object properties hold handles to other objects.  This reference may be strong or weak.  Synthizer currently only uses weak references.
If the reference is weak `syz_handleFree` will set the property internally to NULL.  If the reference is strong `syz_handleFree` will hide the object from the external world, but will keep the object alive.
Synthizer may use strong properties for cases such as `BufferGenerator`, where it is advantageous for the library to be able to control the lifetime.

Property writes are always ordered with respect to other property writes on the same thread, and in general work how you would expect.  Reads are different because Synthizer made the trade-off of low latency and fast writes for fast and consistent reads, and internally uses a number of lockfree ringbuffers to move writes between threads.  Synthizer reserves the right to make property reads eventually consistent.  Synthizer reserves the right to make property reads unordered with respect to their writes even in cases of only one thread doing both reads and writes.
`syz_setX(obj, property, value)` followed by a read to the same property may or may not return the value just written.  Synthizer plans to offer some limited forms of atomicity and database-like transactions in future, which may freeze and/or rewind time with respect to external reads. Note that implicit property reads done by Synthizer itself as part of i.e. source creation are ordered with respect to writes in the expected manner.

If you got lost in the above paragraph: `syz_setX` does what you intuitively expect, `syz_getX` might or might not do what you expect.

The actual links between properties and objects are specified in this manual, and also in `synthizer_properties.h` as a collection of x-macros. The names and contents of the x-macros are unstable, but will be stabilized before 1.0 in order to open up interesting use cases to C developers who might want reflection-like capabilities, i.e. generating UIs.
