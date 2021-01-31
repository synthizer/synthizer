# Events (alpha)

The following functionality is alpha.  In particular, significant changes are expected in the
0.9 release.  For the moment the only documentation is this tutorial; it will be improved after the planned 0.9 changes and
as part of the general pre-1.0 documentation project.

## Introduction

Synthizer supports sending events.  Currently, it can send finished and looped events for both
BufferGenerator and StreamingGenerator.  This will be extended to other objects and concepts in future,
as appropriate.  The events have the following meanings:

- Finished: the generator isn't configured to loop and has reached the end.
- Looped: The generator is configured to loop, and a loop was just completed.

Events are disabled by default and must be enabled.  For C, this means `syz_contextEnableEvents(context)`. For Python, an additional keyword argument `enable_events = True`
can be passed to the Context constructor.

Once enabled, events feed a queue that can be polled with `syz_contextGetNextEvent`.  If events are enabled and the application never polls the queue, the queue will fill up forever; this is
effectively a memory leak.  In other words, only enable events if you know you'll actually use them.

C users are encouraged to read synthizer.h, in particular `struct syz_Event`.  See also the `enum SYZ_EVENT_TYPES` in `synthizer_constants.h`.  The basic idea is to call
`syz_contextGetNextEvent` until you get `SYZ_EVENT_TYPE_INVALID` indicating the end of the queue.

The events system will drop events which would refer to invalid handles on a best-effort basis, so that
an invalid handle is never returned to the caller of `syz_contextGetNextEvent`.  This is guaranteed to work as long as all deletion happens on the same thread as the event polling.  In future, Synthizer is going to switch handles
to a reference-counted scheme which will improve the concurrency story.

## A Python Tutorial

In Python, events are exposed as an iterator on the context:

```
for event in ctx.get_events():
    if isinstance(e, synthizer.FinishedEvent):
        # Handle finished
    elif isinstance(e, synthizer.LoopedEvent):
        # process
```

`get_events` takes an optional argument to limit the number of events returned in one iteration.  By default, it's unlimited.

As shown above, you detect event types with `isinstance`.  Each event has a `source` and `context` property indicating the source (e.g. generator) and context associated with it, as Synthizer objects.
In future, other event types may include more information.

## Notes

Events are currently alpha, as stated above.  Expect bugs.  In particular, there may be pathological cases in which 