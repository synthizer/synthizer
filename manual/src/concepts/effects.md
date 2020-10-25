# Effects and Effect Routing

IMPORTANT: the effects API is provisional and subject to change for the time being.  At the moment this is a hopefully final rough draft of the functionality, but experience is
required to determine if it can be stabilized in the current form.

Synthizer will support two kinds of effects: global effects and generator-specific effects.  At the moment, only global effects are implemented.

users of the Synthizer API can route any number of sources to any number of global effects, for example [echo](../object_reference/echo.md).  This is done through the following C API:

```
struct RouteConfig {
	float gain;
	float fade_time;
};

SYZ_CAPI syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, struct RouteConfig *config);
SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, float fade_out);
```

Routes are uniquely identified by the output object (Source3D, etc) and input object (Echo, etc).  There is no route handle
type, nor is it possible to form duplicate routes.

In order to establish or update the parameters of a route, use `syz_routingConfigRoute`.  This will form a route if there wasn't already one, and update the parameters
as necessary.

In order to remove a route, use `syz_routingRemoveRoute`.

Both of these functions support crossfading provided in seconds.  Internally, this is truncated to the nearest block.  As an exception to this rule, non-zero fade times always give at least one block,
under the assumption that if some fading was requested the goal was to avoid clipping.  Specifically, in pseudocode:

```
blocks = truncate(fade * SR / BLOCK_SIZE)
if blocks == 0 and fade != 0:
    blocks = 1
```


This manual doesn't document global effects as distinct entities because Synthizer is internally designed to allow for object reuse in future when we support
per-generator effects.  Specifically, an object like [Echo](../object_reference/echo.md) will often be able to be used in both positions.
