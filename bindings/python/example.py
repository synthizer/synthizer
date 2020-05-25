import synthizer
import time
import sys

if len(sys.argv) != 2:
    print("Usage: example.py <file>")
    sys.exit(1)

synthizer.initialize()
ctx = synthizer.Context()
g = synthizer.StreamingGenerator(context=ctx, protocol="file", path=sys.argv[1])
s = synthizer.PannedSource(ctx)
s.add_generator(g)

