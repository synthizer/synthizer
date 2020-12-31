# Objects, Handles, and Properties

Synthizer represents references to objects with a `syz_Handle` type.  The following excerpts from `synthizer.h` are available on every object type:

```
SYZ_CAPI syz_ErrorCode syz_handleFree(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle);

SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value);
SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value);
SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);
SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z);
SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2);
```

Synthizer objects are like classes: they have properties, methods, and (optionally) bases. They're created through "constructors", for example `syz_createContext`, and destroyed through `syz_handleFree`.
`syz_handleGetObjectType` can be used to query the type of an object at runtime, returning one of the `SYZ_OTYPE` constants
in `synthizer_constants.h`.

Property names are defined in `synthizer_constants.h` of the form `SYZ_P_FOO`.  Since some objects have common properties and in order to preserve flexibility, the property enum is shared between all objects.

Properties are set through `syz_setX` and read through `syz_getX` where `X` depends on the type:

- `I` for integer
- `D` for double
- `O` for object.
- `D3` for double3, a packed vector of 3 doubles.
- `D6` for a packed array of 6 doubles, commonly used to represent orientations.

Synthizer supports 5 property types: int, double, double3, double6, and object.

Double3 is typically used for position and double6 for orientation.  Synthizer's coordinate system is right-handed, configured so that positive y is forward, positive x east, and positive z up.  Listener orientation is controlled through the context.

Object properties hold handles to other objects.  This is a weak reference, so destroying
the object will set the property to null.  Note that it's not possible to read object properties.  This is because internal machinery
can block the audio thread because locking is required to safely manipulate handles in that case.

Property writes are always ordered with respect to other property writes on the same thread, and in general work how you would expect.  But it's important to note that reads are eventually consistent.  Specifically:

- Two writes from the same thread always happen in the order they were made in terms of audio output.
- The ordering of writes also applies if the app uses synchronization such as mutexes.
- But reads may not return the value just written, and in general return values at some point in the relatively recent past, usually on the order of 5 to 50MS.

It is still useful to read some properties.  An example of this is `SYZ_P_POSITION` on `BufferGenerator`.  Even though Synthizer is returning values that are slightly out of date,
it's still good enough for UI purposes.  Additionally, even if Synthizer always returned the most recent value, audio latency introduces uncertainty as well.  For properties that Synthizer updates, additional effort is made
to keep the latency low enough for practical use, though there is always at least some.

The actual links between properties and objects are specified in this manual.
