"""Demonstrates AngularPannedSource by moving a sound in a circle."""
import sys
import time

import synthizer

with synthizer.initialized():
    ctx = synthizer.Context()
    src = synthizer.AngularPannedSource(
        ctx, panner_strategy=synthizer.PannerStrategy.HRTF
    )
    gen = synthizer.BufferGenerator(ctx)
    src.add_generator(gen)
    gen.looping = True

    buffer = synthizer.Buffer.from_file(sys.argv[1])
    gen.buffer = buffer

    for angle in range(0, 360 * 10):
        src.azimuth = angle % 360
        time.sleep(0.005)
