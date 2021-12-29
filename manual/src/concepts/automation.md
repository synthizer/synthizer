# The Automation API

NOTE: While the intent is that the following is stable, this is provisional and subject to change until things have had
a chance to settle.

## Introduction

Synthizer implements a WebAudio style automation API which can allow for the automation of double, d3, and d6
properties.  This functions through a command queue of automation events, each with a time associated.

In order for applications to know when automation is getting low or for other synchronization events, it is also
possible to enqueue a command which sends an event to your application.

The C API for this funtionality is very large.  Readers should refer to synthizer.h for function and struct prototypes.

## A Note on Accuracy

This is the first cut of the automation API.  Consequently there's a big limitation: it's not possible to perfectly
synchronize automation with the beginning of a generator/source's playback.  That is, as things stand, attempts to build
instruments and keyboards are bound to be disappointing.  Future versions of Synthizer will likely improve this case,
but the primary use for the automation API is fadeouts and crossfades, both of which can be done as things stand today.
Rather than fix this now, this API is being made available to gain experience in order to determine how we want to
address this deficiency in the future.

Improvement for accuracy and the ability to do instrument-level automation is being tracked
[here](https://github.com/synthizer/synthizer/issues/78).

## Overview of usage

The flow of the automation API looks like this:

- get the current time for an object by reading one of the following properties for that object, either from the object
  itself or the context:
  - Using `SYZ_P_CURRENT_TIME` plus a small amount of latency, in order to let the application determine how long it
    needs to get commands enqueued; or
  - `SYZ_P_SUGGESTED_AUTOMATION_TIME` for Synthizer's best suggestion of when to enqueue new commands.
- Build an array of `struct syz_AutomationCommand`.
  - Set the type of command to a `SYZ_AUTOMATION_COMMAND_XXX` constant.
  - Set the time to the time of the command.
  - Set the target to the object to be automated.
  - Set the appropriate command payload in the `params` union.
  - Leave flags at 0, for now.
- get an automation batch with `syz_createAutomationBatch`.
- Add commands to it with `syz_automationbatchAddCommands`.
- Execute the batch with `syz_automationbatchExecute`
- Destroy the batch with `syz_handleDecRef`

The above is involved primarily because this is C: bindings can and should offer easier, builder-like interfaces that
aren't nearly so difficult.

## What's going on?

Automation allows users to build a timeline of events and property values, relative to the context's current time. These
events can then be enqueued for execution as time advances, and will happen within one "block" of the current
time--accurate to about 5MS.  Events can be scheduled in the past, and will contribute to any fading that's going on.

The best way to view this is as an approximation of a function.  That is, if an event is enqueued at time 0 to set
property x to 5, and then at time 10 to set it to 10, then at time 5 the property might be at 5 if the interpolation
type is set to linear.  The key insight is that it doesn't matter when the time 0 event was scheduled: if it was added
at time 4, the value is still 5.

If events are scheduled early, they happen immediately. This is in line with the intended uses of using them to know
when automation is nearing its end or is past specific times.

## Time

Time in Synthizer is always advancing unless the context is paused.  When the context is created, time starts at 0, then
proceeds forward.  It is possible to read the time using `SYZ_P_CURRENT_TIME`, which is available on all objects you'd
expect: generators, source, effects, etc.  By building relative to these object-local times, it is possible for
applications to be prepared for the future when this may no longer be the case, even though they all currently just
defer to the context.

In most cases, though, automating relative to the current time will cause commands to arrive late, and execute in the
past.  To deal with this, Synthizer offers `SYZ_P_SUGGESTED_AUTOMATION_TIME`, the time that Synthizer suggests you might
want to enqueue commands relative to for them to arrive on time.  This also advances forward, but is in the future
relative to `SYZ_P_CURRENT_TIME`.  Currently, this is a simple addition, but the goal is to make the algorithm smarter
in the future.

`SYZ_P_SUGGESTED_AUTOMATION_TIME` is also quite latent.  It is perfectly acceptable for applications to query the
current time, then add their own smaller offset.

There is one typical misuse that people like to do with APIs such as this: don't re-read the current time when building
continuing timelines.  Since the current time is always advancing, this will cause your timeline to be "jagged".  That
is:

```
current_time = # get current time
enqueue(current_time + 5)
current_time = # get current time
enqueue(current_time + 10)
```

Will not produce events 5 seconds apart, but instead 5 seconds and a bit.  Correct usage is:

```
current_time = #get current time
enqueue_command(current_time + 5)
enqueue_command(current_time + 10)
```

A good rule of thumb is that time should be read when building an automation batch, then everything for that batch done
relative to the time that was read at the beginning.

Finally, as implied by the above, automation doesn't respect pausing unless it's on the context: pausing a generator
with automation attached will still have automation advance even while it is paused.  This is currently unavoidable due
to internal limitations which require significant work to lift.

## Automation Batches

In order to enqueue commands, it is necessary to put them somewhere.  We also want an API which allows for enqueueing
more than one command at a time for efficiency.  In order to do this, Synthizer introduces the concept of the automation
batch, created with `syz_createAutomationBatch`.

Automation batches are one-time-use command queues, to which commands can be appended with
`syz_automationBatchAddCommands`.  Uniquely in Synthizer, they are *not* threadsafe: you must provide synchronization
yourself.  Once all the commands you want to add are added, `syz_automationBatchExecute` executes the batch. Afterwords,
due to internal limitations, the batch may not be reused.

Note that the intent is that batches can automate multiple objects at once.  Using a different object with every command
is perfectly fine.

## Commands

Automation offers the following command types:

- `SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY`: Add a value to the timeline for a property, e.g. "use linear interpolation
  to get to 20.0 by 10 seconds".
- `SYZ_AUTOMATION_COMMAND_SEND_USER_EVENT`: Add an event to the timeline, to be sent approximately when the timeline
  crosses the specified time.
- `SYZ_AUTOMATION_COMMAND_CLEAR_PROPERTY`: Clear all events related to one specific property on one specific object.
- `SYZ_AUTOMATION_COMMAND_CLEAR_EVENTS`: Clear all scheduled events for an object.
- `SYZ_AUTOMATION_COMMAND_CLEAR_ALL_PROPERTIES`: Clear automation for all properties for a specific object.

Commands which clear data don't respect time, and take effect immediately.  Typically, they're put at the beginning of
the automation batch in order to clean up before adding new automation.

Parameters to commands are specified as the enum variants of `syz_AutomationCommand`'s `params` union and are mostly
self-explanatory.  Property automation is discussed below.

## Automating Property values

Using `SYZ_AUTOMATION_COMMAND_APPEND_PROPERTY`, it is possible to append values to the timeline for a property on a
specific object.  This is done via the `syz_AutomationPoint` struct:

```
struct syz_AutomationPoint {
  unsigned int interpolation_type;
  double values[6];
  unsigned long long flags;
};
```

The fields are as follows:

- The interpolation type specifies how to get from the last value (if any) to the current value in the point:
  - `SYZ_INTERPOLATION_TYPE_NONE`: do nothing until the point is crossed, then jump immediately.
  - `SYZ_P_INTERPOLATION_TYPE_LINEAR`: use linear interpolation from the last point.
- The values array specifies the values.  The number of indices used depends on the property: doubles use only the first
  index, d3 use the first 3, and d6 use all 6.
- Flags is reserved and must be 0.


The timeline for a specific property represents the approximation of a continuous function, not a list of commands.  As
alluded to above, if points are early they still take effect by changing the continuous function.  This is done so that
things like audio "animations" can execute accurately even if the application experiences a delay.

Property automation interacts with normal property sets by desugaring normal property sets to
`SYZ_INTERPOLATION_TUYPE_NONE` points scheduled "as soon as possible".  Users are encouraged to either use automation or
property setting rather than both at the same time, but both at the same time has a consistent behavior.

## Events

Events are much simpler: you specify a `unsigned long long` parameter, and get it back when the event triggers.
`unsigned long long` is used because it is possible to losslessly store a pointer in it on all platforms we support
without having to worry about the size changing on 32-bit platforms.

## Examples.

Synthizer's repository contains some examples demonstrating common automation uses in the examples directory:

- `simple_automation.c`: build a simple envelope around a waveform.
- `automation_circle.c`: automate the position of a source to move it in circles.
- `play_notes.c`: play some notes every time the user presses enter (this works because the waveform is constant, but
  isn't ideal as explained above).
