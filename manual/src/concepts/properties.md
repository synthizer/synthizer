# Controlling Object Properties

## Basics

most interesting audio control happens through properties, which are like knobs
on hardware controllers or dials in your DAW.  Synthizer picks the values of
properties up on the next audio tick and automatically handles crossfading and
graceful changes as your app drives the values.  Every property is identified
with a `SYZ_P` constant in `synthizer_constants.h`.  IN bindings,
`SYZ_P_MY_PROPERTY` will generally become `my_property` or `MyProperty` or etc.
depending on the dominant style of the language, and then either become an
actual settable property or a `get_property` and `set_property` pair depending
on if the language in question supports customized properties that aren't just
member variables (e.g. `@property` in Python, properties in C#).

All properties are of one of the following types:

- `int` or `double`, identified by a `i` and `d` suffix in the property API, the
  standard C primitive types.
- `double3` and `double6`, identified by `d3` and `d6` suffixes, vectors of 3
  doubles and 6 doubles respectively.  Primarily used to set position and
  orientation.
- `object`, identified by a `o` suffix, used to set object properties such as
  the buffer to use for a buffer generator.
- `biquad`, configuration for a biquad filter.  Used on effects and sources to allow filtering audio.

No property constant represents a property of two types.  For example
`SYZ_P_POSITION` is both on `Context` and `Source3D` but is a `d3` in both
cases.  Generators use `SYZ_P_PLAYBACK_POSITION`, which is always a double.
Synthizer will always maintain this constraint.

The Property API is as follows:

```
SYZ_CAPI syz_ErrorCode syz_getI(int *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setI(syz_Handle target, int property, int value);

SYZ_CAPI syz_ErrorCode syz_getD(double *out, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD(syz_Handle target, int property, double value);

SYZ_CAPI syz_ErrorCode syz_setO(syz_Handle target, int property, syz_Handle value);

SYZ_CAPI syz_ErrorCode syz_getD3(double *x, double *y, double *z, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD3(syz_Handle target, int property, double x, double y, double z);

SYZ_CAPI syz_ErrorCode syz_getD6(double *x1, double *y1, double *z1, double *x2, double *y2, double *z2, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setD6(syz_Handle handle, int property, double x1, double y1, double z1, double x2, double y2, double z2);

SYZ_CAPI syz_ErrorCode syz_getBiquad(struct syz_BiquadConfig *filter, syz_Handle target, int property);
SYZ_CAPI syz_ErrorCode syz_setBiquad(syz_Handle target, int property, const struct syz_BiquadConfig *filter);
```

Property accesses happen without syscalls and are usually atomic operations and
enqueues on a lockfree queue.

## Object Properties Are Weak

Object properties do not increment the reference count of the handle associated
with them.  There isn't much to say here, but it is important enough that it's
worth calling out with a section.  For example, if you set the buffer on a
buffer generator and then decrement the buffer's reference count to 0, the
generator will stop playing audio rather than keeping the buffer alive.

## A Note on Reading

Property reads need to be further explained.  Because audio programming requires
not blocking the audio thread, Synthizer internally uses queues for property
writes.  This means that any read may be outdated by some amount, even if the
thread making the read just set the value.  Typically, reads should be reserved
for properties that Synthizer also sets (e.g. `SYZ_P_PLAYBAKCK_POSITION`) or
used for debugging purposes.

`syz_getO` is not offered by this API because it requires a mutex, which the
audio thread also can't handle.  Additionally, object lifetime concerns make it
more difficult for such an interface to do something sane.

Though the above limitations prevent this anyway, it is in general an
antipattern to store application state in your audio library.  Even if reads
were always up to date, it would still be slow to get data back out.
Applications should keep things like object position around and update
Synthizer, rather than asking Synthizer what the last value was.