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
#buffer = synthizer.Buffer.from_stream_params("file", sys.argv[1])
#gen = synthizer.BufferGenerator(ctx)
gen = synthizer.StreamingGenerator.from_file(ctx, sys.argv[1])

# gen.buffer = buffer
# ctx.panner_strategy = synthizer.PannerStrategy.HRTF
src = synthizer.PannedSource(ctx)
src.add_generator(gen)
src.panner_strategy = synthizer.PannerStrategy.HRTF

#reverb = synthizer.GlobalFdnReverb(ctx)
echo=synthizer.GlobalEcho(ctx)
tap = synthizer.EchoTapConfig(gain_l=1.0, gain_r=1.0, delay=2.0)
echo.set_taps([tap])
ctx.config_route(src, echo)
echo.configure_delete_behavior(linger=True)

