"""A simple media player, demonstrating the Synthizer basics."""

import synthizer
import time
import sys

if len(sys.argv) != 2:
    print("Usage: example.py <file>")
    sys.exit(1)

# Log to debug. At the moment this writes directly to stdout, but will in future integrate with Python's logging modules.
# It's best to call this before any initialization.
synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

with synthizer.initialized():
    # Get our context, which almost everything requires.
    # This starts the audio threads.
    ctx = synthizer.Context()

    # A BufferGenerator plays back a buffer:
    generator = synthizer.BufferGenerator(context=ctx)
    # A buffer holds audio data. We read from the specified file:
    buffer = synthizer.Buffer.from_stream(ctx, protocol="file", path=sys.argv[1])
    # Tell the generator to use the buffer.
    generator.buffer = buffer
    # A Source3D is a 3D source, as you'd expect.
    source = synthizer.Source3D(ctx)
    # It'll play the BufferGenerator.
    source.add_generator(generator)
    playing = True

    # A simple command parser.
    while True:
        cmd = input("Command: ")
        cmd = cmd.split()
        if len(cmd) == 0:
            continue
        if cmd[0] == "pause":
                # We don't have proper pausing yet, but can simulate it
                if playing:
                    source.remove_generator(generator)
                    playing = False
        elif cmd[0] == "play":
            if not playing:
                source.add_generator(generator)
                playing = True
        elif cmd[0] == "pos":
            if len(cmd) < 4:
                print("Syntax: pos x y z")
                continue
            try:
                x, y, z = [float(i) for i in cmd[1:]]
            except:
                print("Unable to parse coordinates")
                continue
            source.position = (x, y, z)
        elif cmd[0] == "seek":
            if len(cmd) != 2:
                print("Syntax: pos <seconds>")
            try:
                pos = float(cmd[1])
            except:
                print("Unable to parse position")
            generator.position = pos
        elif cmd[0] == "quit":
            break
        else:
            print("Unrecognized command")
