"""Demonstrates a custom stream protocols by wrapping a file object.  Call as:

custom_stream.py filename
"""
import sys

import synthizer


class CustomStream:
    def __init__(self, path):
        self.file = open(path, "rb")

    def read(self, size):
        return self.file.read(size)

    def seek(self, position):
        self.file.seek(position)

    def close(self):
        self.file.close()

    def get_length(self):
        pos = self.file.tell()
        len = self.file.seek(0, 2)
        self.file.seek(pos)
        return len


def factory(protocol, path, param):
    return CustomStream(path)


with synthizer.initialized(
    log_level=synthizer.LogLevel.DEBUG, logging_backend=synthizer.LoggingBackend.STDERR
):
    ctx = synthizer.Context()
    src = synthizer.DirectSource(ctx)

    # There are two ways to get a custom stream. This example demonstrates registering a protocol:
    synthizer.register_stream_protocol("custom", factory)
    gen = synthizer.StreamingGenerator.from_stream_params(ctx, "custom", sys.argv[1])
    # but you could also:
    # sh = synthizer.StreamHandle.from_custom_stream(CustomStream(sys.argv[1]))
    # gen = synthizer.StreamingGenerator.from_stream_handle(ctx, sh)
    gen.looping = True
    src.add_generator(gen)

    print("Press enter to exit...")
    input()
