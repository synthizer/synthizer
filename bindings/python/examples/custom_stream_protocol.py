"""Demonstrates registering a custom stream protocol by wrapping a file object.  Call as:

custom_stream_protocol.py filename
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

    synthizer.register_stream_protocol("custom", factory)
    gen = synthizer.StreamingGenerator.from_stream_params(ctx, "custom", sys.argv[1])
    gen.looping = True
    src.add_generator(gen)

    print("Press enter to exit...")
    input()
