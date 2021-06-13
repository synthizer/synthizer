"""Read a file into memory in Python, then ask the buffer to decode it for us.
Demonstrates how to decode encoded data from places that aren't the disk, e.g.
the result of an http request.

usage: buffer_from_memory.py myfile."""
import sys

import synthizer

with synthizer.initialized():
    ctx = synthizer.Context()
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    buffer = synthizer.Buffer.from_encoded_data(data)
    gen = synthizer.BufferGenerator(ctx)
    gen.buffer = buffer
    src = synthizer.DirectSource(ctx)
    src.add_generator(gen)
    print("Press enter to exit")
    input()
