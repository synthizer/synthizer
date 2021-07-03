# Effects and Effect Routing

users of the Synthizer API can route any number of sources to any number of
global effects, for example [echo](../object_reference/global_echo.md).  This is done
through the following C API:

```
struct syz_RouteConfig {
    float gain;
    float fade_time;
    syz_BiquadConfig filter;
};

SYZ_CAPI syz_ErrorCode syz_initRouteConfig(struct syz_RouteConfig *cfg);
SYZ_CAPI syz_ErrorCode syz_routingConfigRoute(syz_Handle context, syz_Handle output, syz_Handle input, struct syz_RouteConfig *config);
SYZ_CAPI syz_ErrorCode syz_routingRemoveRoute(syz_Handle context, syz_Handle output, syz_Handle input, float fade_out);
```

Routes are uniquely identified by the output object (Source3D, etc) and input
object (Echo, etc).  There is no route handle type, nor is it possible to form
duplicate routes.

In order to establish or update the parameters of a route, use
`syz_routingConfigRoute`.  This will form a route if there wasn't already one,
and update the parameters as necessary.

It is necessary to initialize `syz_RouteConfig` with `syz_initRouteConfig`
before using it, but this need only be done once.  After that, reusing the same
`syz_RouteConfig` for a route without reinitializing it is encouraged.

Gains are per route and apply after the gain of the source. For example, you
might feed 70% of a source's output to something (gain = 0.7).

Filters are also per route and apply after any filters on sources.  For example,
this can be used to change the filter on a per-reverb basis for a reverb zone
algorithm that feeds sources to more than one reverb at a time.

In order to remove a route, use `syz_routingRemoveRoute`.

Many effects involve feedback and/or other long-running audio as part of their
intended function. But while in development, it is often useful to reset an
effect.  Synthizer exposes a function for this purpose:

```
SYZ_CAPI syz_ErrorCode syz_effectReset(syz_Handle effect);
```

Which will work on any effect (at most, it does nothing).  As with things like
property access this is slow, and it's also not going to sound good, but it can
do things like clear out the feedback paths of a reverb at the Python shell for
interactive experimentation purposes.
