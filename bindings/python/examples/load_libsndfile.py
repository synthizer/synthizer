"""Example demonstrating how to load Libsndfile. To use, provide two
parameters: load_libsndfile.py libsndfile_path file_path"""
import sys

import synthizer

libsndfile_path = sys.argv[1]
file_path = sys.argv[2]


with synthizer.initialized(
    log_level=synthizer.LogLevel.DEBUG,
    logging_backend=synthizer.LoggingBackend.STDERR,
    libsndfile_path=libsndfile_path,
):
    ctx = synthizer.Context()
    buffer = synthizer.Buffer.from_file(file_path)
    gen = synthizer.BufferGenerator(ctx)
    gen.buffer = buffer
    src = synthizer.DirectSource(ctx)
    src.add_generator(gen)
    print("Press enter to exit")
    input()
