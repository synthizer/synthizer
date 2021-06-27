"""A simple media player, demonstrating the Synthizer basics."""

import sys

import synthizer

if len(sys.argv) != 2:
    print(f"Usage: {sys.argv[0]} <file>")
    sys.exit(1)

with synthizer.initialized(
    log_level=synthizer.LogLevel.DEBUG, logging_backend=synthizer.LoggingBackend.STDERR
):
    # Get our context, which almost everything requires.
    # This starts the audio threads.
    ctx = synthizer.Context()
    # Enable HRTF as the default panning strategy before making a source
    ctx.default_panner_strategy = synthizer.PannerStrategy.HRTF

    # A BufferGenerator plays back a buffer:
    generator = synthizer.BufferGenerator(ctx)
    # A buffer holds audio data. We read from the specified file:
    buffer = synthizer.Buffer.from_file(sys.argv[1])
    # Tell the generator to use the buffer.
    generator.buffer = buffer
    # A Source3D is a 3D source, as you'd expect.
    source = synthizer.Source3D(ctx)
    # It'll play the BufferGenerator.
    source.add_generator(generator)
    # Keep track of looping, since property reads are expensive:
    looping = False

    # A simple command parser.
    while True:
        cmd = input("Command: ")
        cmd = cmd.split()
        if len(cmd) == 0:
            continue
        if cmd[0] == "pause":
            source.pause()
        elif cmd[0] == "play":
            source.play()
        elif cmd[0] == "pos":
            if len(cmd) < 4:
                print("Syntax: pos x y z")
                continue
            try:
                x, y, z = [float(i) for i in cmd[1:]]
            except ValueError:
                print("Unable to parse coordinates")
                continue
            source.position = (x, y, z)
        elif cmd[0] == "seek":
            if len(cmd) != 2:
                print("Syntax: seek <seconds>")
                continue
            try:
                pos = float(cmd[1])
            except ValueError:
                print("Unable to parse position")
                continue
            try:
                generator.playback_position = pos
            except synthizer.SynthizerError as e:
                print(e)
        elif cmd[0] == "quit":
            break
        elif cmd[0] == "loop":
            looping = not looping
            generator.looping = looping
            print("Looping" if looping else "Not looping")
        elif cmd[0] == "gain":
            if len(cmd) != 2:
                print("Syntax: gain <value>")
                continue
            try:
                value = float(cmd[1])
            except ValueError:
                print("Unable to parse value.")
                continue
            # Convert to scalar gain from db.
            gain = 10 ** (value / 20)
            source.gain = gain
        else:
            print("Unrecognized command")
