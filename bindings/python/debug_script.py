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

synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

synthizer.initialize()
ctx = synthizer.Context()
gen = synthizer.BufferGenerator(ctx)
buffer = synthizer.Buffer.from_stream("file", sys.argv[1], "")
gen.buffer = buffer
src = synthizer.DirectSource(ctx)
src.gain=0.2
src.add_generator(gen)
