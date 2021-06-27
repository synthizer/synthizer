# Events

Synthizer supports sending events.  Currently, it can send finished and looped
events for both `BufferGenerator` and `StreamingGenerator`.  This will be
extended to other objects and concepts in future, as appropriate.  The events
have the following meanings:

- Finished: the generator isn't configured to loop and has reached the end.
- Looped: The generator is configured to loop, and a loop was just completed.

Events are disabled by default and must be enabled.  An additional keyword
argument `enable_events = True` can be passed to the Context constructor.

Once enabled, events feed a queue that must be polled forever.  If events are
enabled and the application never polls the queue, the queue will fill up
forever; this is effectively a memory leak.  In other words, only enable events
if you know you'll actually use them.

The events system will drop events which would refer to a destroyed object.
Destroying a generator before seeing its finished event, for instance, means
you'll never see it.

Events are exposed as an iterator on the context:

```
for event in ctx.get_events():
    if isinstance(e, synthizer.FinishedEvent):
        # Handle finished
    elif isinstance(e, synthizer.LoopedEvent):
        # process
```

The iterator returned from `get_events` takes an optional argument to limit the
number of events returned in one iteration.  By default, it's unlimited, and
will stop returning events as soon as none are available (in other words: it
never blocks and can be used in game loops).

As shown above, you detect event types with `isinstance`.  Each event has a
`source` and `context` property indicating the source (e.g. generator) and
context associated with it, as Synthizer objects. In future, other event types
may include more information.
