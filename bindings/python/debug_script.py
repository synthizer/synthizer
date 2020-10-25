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

synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

synthizer.initialize()
ctx = synthizer.Context()
gen = synthizer.BufferGenerator(ctx)
buffer = synthizer.Buffer.from_stream("file", sys.argv[1], "")
gen.buffer = buffer
src = synthizer.Source3D(ctx)
src.add_generator(gen)

echo = synthizer.GlobalEcho(ctx)
ctx.config_route(src, echo)

n_taps = 100
duration = 2.0
delta = duration / n_taps
taps = [
    EchoTapConfig(delta + i * delta + random.random() * 0.01, random.random(), random.random())
    for i in range(n_taps)
]

norm_left = sum([i.gain_l** 2 for i in taps])
norm_right = sum([i.gain_r ** 2 for i in taps])
norm = 1.0 / math.sqrt(max(norm_left, norm_right))
for t in taps:
    t.gain_l *= norm
    t.gain_r *= norm

echo.set_taps(taps)
