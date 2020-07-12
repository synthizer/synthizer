# Used so that I can get a quick python -i up for today's debugging session. You don't want to learn from this code; it may not even work.
import synthizer
import time
import sys
import atexit

# Normally you want to use the synthizer.initialized context manager, but I'm using this example
# as a script that sets up a Python shell for debugging, and I
# forgot to shut this down and had to kill via task manager one too many times.
#
# You always need to shut Synthizer down, but I'll be improving things so that failing to do so
# doesn't freeze things so badly that you have to kill it via task manager.
atexit.register(synthizer.shutdown)
if len(sys.argv) != 2:
    print("Usage: example.py <file>")
    sys.exit(1)

synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

synthizer.initialize()
ctx = synthizer.Context()
b = synthizer.Buffer.from_stream(ctx, "file", sys.argv[1], "")

def sgpair():
    g = synthizer.BufferGenerator(context=ctx)
    g.buffer = b    
    s = synthizer.Source3D(ctx)
    s.add_generator(g)
    return s, g



pairs = []
for i in range(-8, 9, 1):
    s, g = sgpair()
    s.position=i, 0, 0
    s.gain=-9
    pairs.append((s, g))
    time.sleep(1)


time.sleep(10)
