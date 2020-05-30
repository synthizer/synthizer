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
g = synthizer.StreamingGenerator(context=ctx, protocol="file", path=sys.argv[1])
s = synthizer.PannedSource(ctx)
s.add_generator(g)

