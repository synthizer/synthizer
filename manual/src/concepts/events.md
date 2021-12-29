# Events

Synthizer supports receiving events.  Currently, this is limited to knowing when
buffer/streaming generators have looped and/or finished.  Note that the use case
of destroying objects only after they have stopped playing is better handled
with [lingering](./lingering.md).

The API for this is as follows:

```
struct syz_Event {
    int type;
    syz_Handle source;
    syz_Handle context;
};

SYZ_CAPI syz_ErrorCode syz_contextEnableEvents(syz_Handle context);
SYZ_CAPI syz_ErrorCode syz_contextGetNextEvent(struct syz_Event *out, syz_Handle context, unsigned long long flags);

SYZ_CAPI void syz_eventDeinit(struct syz_Event *event);
```

To begin receiving events, an application should call `syz_contextEnableEvents`.
This cannot be undone.  After a call to `syz_contextEnableEvents`, events will
begin to fill the event queue and must be retrieved with
`syz_contextGetNextEvent`.  Failure to call `syz_contextGetNextEvent` will
slowly fill the event queue, so applications should be sure to incorporate this
into their main UI/game update loops.  After the application is done with an
event struct, it should then call `syz_eventDeinit` on the event structure;
failure to do so leaks handles.

The `flags` argument of `syz_getNextEvent` is reserved and must be 0.

Events have a type, context, and source.  The type is the kind of the vent.  The
context is the context from which the event was extracted.  The source is the
source of the event.  Sources are not sources as in the Synthizer object, and
are actually most commonly generators.

Event type constants are declared in `synthizer_constants.h` with all other
constants.  Currently Synthizer only offers `SYZ_EVENT_TYPE_FINISHED` and
`SYZ_EVENT_TYPE_LOOPED` which do exactly what they sound like: finished fires
when a generator which isn't configured to loop finished, and looped every time
a looping generator resets.

A special event type constant, `SYZ_EVENT_TYPE_INVALID`, is returned by
`syz_contextGetNextEvent` when there are no events in the queue.  To write a
proper event loop (excluding error handling):

```
struct syz_Event evt;

while(1) {
    syz_contextGetNextEvent(&evt, context, 0);
    if (evt.type == SYZ_EVENT_TYPE_INVALID) {
        break;
    }
    // handle it
}
```

Synthizer will never return an event if any handle to which the event refers is
invalid at the time the event was extracted from the queue.  This allows
applications to delete handles without having to concern themselves with whether
or not an event refers to a deleted handle.

In order to also offer thread safety, Synthizer event handling will temporarily
increment the reference counts of any handles to which an event refers, then
decrement them when `syz_eventDeinit` is called.  This allows applications the
ability to delete objects on threads other than the thread handling the event,
at the cost of extending the lifetimes of these handles slightly.  It is
possible for an application to call `syz_handleIncRef` if the application wishes
to keep one of these handles around.
