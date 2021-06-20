# Handles and userdata

Synthizer objects are referred to via reference-counted handles, with an
optional `void *` userdata pointer that can be associated to link them to
application state:

```
SYZ_CAPI syz_ErrorCode syz_handleIncRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleDecRef(syz_Handle handle);
SYZ_CAPI syz_ErrorCode syz_handleGetObjectType(int *out, syz_Handle handle);

SYZ_CAPI syz_ErrorCode syz_getUserdata(void **out, syz_Handle handle);
typedef void syz_UserdataFreeCallback(void *);
SYZ_CAPI syz_ErrorCode syz_setUserdata(syz_Handle handle, void *userdata, syz_UserdataFreeCallback *free_callback);
```

All Synthizer handles start with a reference count of 1.  When the reference
count reaches 0, the object is scheduled for deletion, but may not be deleted
immediately.

Synthizer objects are like classes.  They have "methods" and "bases".  For
example all generators support a common set of operations named with a
`syz_generatorXXX` prefix.

Getting and setting userdata pointers is done through `syz_getUserdata` and
`syz_setUserdata`.  These are a threadsafe interface which will associate a
`void *` argument with the object.  This interface acts as if the operations
were wrapped in a mutex internally, though they complete with no syscalls in all
reasonable cases of library usage.

The `free_callback` parameter to `syz_setUserdata` is optional.  If present, it
will be called on the userdata pointer when the object is destroyed or when a
new userdata pointer is set.  Due to limitations of efficient audio programming,
this free happens in a background thread and may occur up to hundreds of
milliseconds after the object no longer exists.

Most bindings will bind userdata support in a more friendly way.  For example,
Python provides a `get_userdata` and `set_userdata` pair which work on normal
Python objects.