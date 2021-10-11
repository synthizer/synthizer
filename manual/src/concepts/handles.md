# Handles and userdata

Synthizer objects are referred to via reference-counted handles, with an optional `void *` userdata pointer that can be
associated to link them to application state:

```
SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle);

SYZ_CAPI syz_ErrorCode syz_handleGetUserdata(void **out, syz_Handle handle);
typedef void syz_UserdataFreeCallback(void *);
SYZ_CAPI syz_ErrorCode syz_handleSetUserdata(syz_Handle handle, void *userdata, syz_UserdataFreeCallback *free_callback);
```

## basics of Handles

All Synthizer handles start with a reference count of 1.  When the reference count reaches 0, the object is scheduled
for deletion, but may not be deleted immediately.  Uniquely among Synthizer functions, `syz_handleIncRef` and
`syz_handleDecRef` can be called after library shutdown in order to allow languages like Rust to implement infallible
cloning and freeing.  The issues introduced with respect to object lifetimes due to the fact that Synthizer objects may
stay around for a while can be dealt with userdata support, as described below.

Synthizer objects are like classes.  They have "methods" and "bases".  For example all generators support a common set
of operations named with a `syz_generatorXXX` prefix.

### The reserved config argument

Many constructors take a `void *config` argument.  This must be set to NULL, and is reserved for future use.

### Userdata

Synthizer makes it possible to associate application data via a `void *` pointer which will share the object's actual
lifetime rather than the lifetime of the handle to the object.  This is useful for allowing applications to store state,
but also helps to deal with the lifetime issues introduced by the mismatch between the last reference count of the
object dying and the object actually dying.  For example, the Rust and Python bindings use userdata to attach buffers to
objects when streaming from memory, so that the actual underlying resource stays around until Synthizer is guaranteed to
no longer use it.

Getting and setting userdata pointers is done in one of two ways.  All Synthizer constructors take two additional
parameters to set the userdata and the free callback.  Alternatively, an application can go through `syz_getUserdata`
and `syz_setUserdata`.  These are a threadsafe interface which will associate a `void *` argument with the object.  This
interface acts as if the operations were wrapped in a mutex internally, though they complete with no syscalls in all
reasonable cases of library usage.

The `free_callback` parameter to `syz_setUserdata` is optional.  If present, it will be called on the userdata pointer
when the object is destroyed or when a new userdata pointer is set.  Due to limitations of efficient audio programming,
this free happens in a background thread and may occur up to hundreds of milliseconds after the object no longer exists.

Most bindings will bind userdata support in a more friendly way.  For example, Python provides a `get_userdata` and
`set_userdata` pair which work on normal Python objects.
