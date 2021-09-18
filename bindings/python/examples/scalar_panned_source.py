"""Demonstrates ScalarPannedSource by moving a sound back and forth. """
import sys
import time

import synthizer

with synthizer.initialized():
    ctx = synthizer.Context()
    src = synthizer.ScalarPannedSource(
        ctx, panner_strategy=synthizer.PannerStrategy.HRTF, panning_scalar=-1.0
    )
    gen = synthizer.BufferGenerator(ctx)
    src.add_generator(gen)
    gen.looping = True

    buffer = synthizer.Buffer.from_file(sys.argv[1])
    gen.buffer = buffer

    iterations = 100
    steps_per_iteration = 100
    half_iteration = steps_per_iteration // 2
    sleep_time = 0.02
    for i in range(iterations * steps_per_iteration):
        iter_offset = i % steps_per_iteration
        half_iter_offset = i % half_iteration
        offset = 2.0 * half_iter_offset / half_iteration
        value = -1.0 + offset
        if iter_offset >= half_iteration:
            value *= -1.0
        src.panning_scalar = value
        time.sleep(sleep_time)
