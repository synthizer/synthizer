"""Demonstrates how to create a buffer from a `StreamHandle`. To use, pass the
path to a file."""
import sys

import synthizer

with synthizer.initialized():
    ctx = synthizer.Context()
    sh = synthizer.StreamHandle.from_file(sys.argv[1])
    gen = synthizer.StreamingGenerator.from_stream_handle(ctx, sh)
    src = synthizer.DirectSource(ctx)
    src.add_generator(gen)
    print("Press any key to exit")
    input()
