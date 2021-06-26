# Configuring Objects to Continue Playing Until Silent

By default, Synthizer objects become silent when their reference counts go to 0,
but this isn't always what you want.  Sometimes, it is desirable to be able to
continue playing audio until the object is "finished", for example for gunshots
or other one-off effects.  Synthizer calls this lingering, and offers the
following API to configure it:

```
struct syz_DeleteBehaviorConfig {
    int linger;
    double linger_timeout;
};

SYZ_CAPI void syz_initDeleteBehaviorConfig(struct syz_DeleteBehaviorConfig *cfg);
SYZ_CAPI syz_ErrorCode syz_configDeleteBehavior(syz_Handle object, struct syz_DeleteBehaviorConfig *cfg);
```

To use it, call `syz_initDeleteBehaviorConfig` on an empty
`syz_DeleteBehaviorConfig` struct, fill out the struct, and call
`syz_configDeleteBehavior`.  The fields have the following meaning:

- `linger`: if 0, die immediately, which is the default.  If 1, keep the object
  around until it "finishes".  What this means depends on the object and is
  documented in the object reference, but it generally "does what you'd expect".
  For some examples:
  - `BufferGenerator` will stop any looping and play until the end of the
    buffer, or die immediately if paused.
  - All sources will keep going until all their generators are no longer around.
- `linger_timeout`: if nonzero, set an upper bound on the amount of time an
  object may linger for.  This is useful as a sanity check in your application.

Lingering doesn't keep related objects alive.  For example a `BufferGenerator`
that is lingering still goes silent if the buffer attached to it is destroyed.

As with pausing, bindings usually make this an instance method.
