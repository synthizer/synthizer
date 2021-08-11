# Used so that I can get a quick python -i up for today's debugging session. You don't want to learn from this code; it may not even work.
import synthizer
from synthizer import EchoTapConfig

import time
import random
import sys
import math
import atexit

# Normally you want to use the synthizer.initialized context manager, but I'm using this example
# as a script that sets up a Python shell for debugging, and I
# forgot to shut this down and had to kill via task manager one too many times.
#
# You always need to shut Synthizer down, but I'll be improving things so that failing to do so
# doesn't freeze things so badly that you have to kill it via task manager.
atexit.register(synthizer.shutdown)

synthizer.initialize(
    log_level=synthizer.LogLevel.DEBUG, logging_backend=synthizer.LoggingBackend.STDERR
)
ctx = synthizer.Context(enable_events=True)
buffer = synthizer.Buffer.from_stream_params("file", sys.argv[1])
gen = synthizer.BufferGenerator(ctx)
gen2 = synthizer.BufferGenerator(ctx)
#gen = synthizer.StreamingGenerator.from_file(ctx, sys.argv[1])
#gen2 = synthizer.StreamingGenerator.from_file(ctx, sys.argv[1])

gen.buffer = buffer
gen2.buffer=buffer
# ctx.panner_strategy = synthizer.PannerStrategy.HRTF
src = synthizer.PannedSource(ctx)
src.add_generator(gen)
src.add_generator(gen2)
gen.config_delete_behavior(linger=True)
gen2.config_delete_behavior(linger=True)
src.config_delete_behavior(linger=True)
gen.dec_ref()
gen2.dec_ref()
src.dec_ref()
#src.panner_strategy = synthizer.PannerStrategy.HRTF
