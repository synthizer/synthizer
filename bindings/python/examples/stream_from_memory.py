"""Demonstrates how to stream from encoded data in memory. To use, pass a filename as the argument."""
import sys

import synthizer

with synthizer.initialized():
    ctx = synthizer.Context()
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    sh = synthizer.StreamHandle.from_memory(data)
    gen = synthizer.StreamingGenerator.from_stream_handle(ctx, sh)
    src = synthizer.DirectSource(ctx)
    src.add_generator(gen)
    print("Press enter to exit")
    input()
