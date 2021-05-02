"""Demonstrates how to configure echo in a  relatively real-world scenario akin to what a reverb would do for early reflections.

By setting taps up in various configurations, it's possible to get a number of different effects.

Usage is echo.py myfile.
For best results, use a music file or something that's long enough to play as long as the sleeps at the end of this file."""
import synthizer

import time
import random
import sys
import math

with synthizer.initialized(
    log_level=synthizer.LogLevel.DEBUG, logging_backend=synthizer.LoggingBackend.STDERR
):
    # Normal source setup from a CLI arg.
    ctx = synthizer.Context()
    gen = synthizer.BufferGenerator(ctx)
    buffer = synthizer.Buffer.from_file(sys.argv[1])
    gen.buffer = buffer
    src = synthizer.Source3D(ctx)
    src.add_generator(gen)

    # create and connect the effect with a default gain of 1.0.
    echo = synthizer.GlobalEcho(ctx)
    ctx.config_route(src, echo)

    # Generate uniformly distributed random taps.
    # Remember: echo currently only allows up to 5 seconds of delay.
    n_taps = 100
    duration = 2.0
    delta = duration / n_taps
    taps = [
        synthizer.EchoTapConfig(
            delta + i * delta + random.random() * 0.01, random.random(), random.random()
        )
        for i in range(n_taps)
    ]

    # In general, you'll want to normalize by something, or otherwise work out how to prevent clipping.
    # Synthizer as well as any other audio library can't protect from clipping due to too many sources/loud effects/etc.
    # This script normalizes so that the constant overall power of the echo is around 1.0, but
    # a simpler strategy is to simply compute an average.  Wich works better depends
    # highly on the use case.
    norm_left = sum([i.gain_l ** 2 for i in taps])
    norm_right = sum([i.gain_r ** 2 for i in taps])
    norm = 1.0 / math.sqrt(max(norm_left, norm_right))
    for t in taps:
        t.gain_l *= norm
        t.gain_r *= norm

    echo.set_taps(taps)

    # Sleep for a bit, to let the audio be heard
    time.sleep(10.0)
    # Set the source's gain to 0, which will let the tail of the echo be heard.
    src.gain = 0.0
    # Sleep for a bit for the tail.
    time.sleep(5.0)
    # Bring it back. This causes a little bit of clipping because of the abrupt change.
    src.gain = 1.0
    # Sleep for long enough to build up audio in the echo:
    time.sleep(5.0)
    # Fade the send out over the next 1 seconds:
    ctx.remove_route(src, echo, fade_time=1.0)
    time.sleep(2.0)
