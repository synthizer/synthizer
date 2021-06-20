# Pausing and Resuming Playback

All objects which play audio offer the following two functions:

```
SYZ_CAPI syz_ErrorCode syz_pause(syz_Handle object);
SYZ_CAPI syz_ErrorCode syz_play(syz_Handle object);
```

Which do exactly what they seem like they do.

In bindings, these are usually bound as instance methods, e.g. `myobj.pause()`.
