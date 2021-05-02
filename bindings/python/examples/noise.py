import synthizer
import time
import itertools

noise_types = [synthizer.NoiseType.UNIFORM, synthizer.NoiseType.VM]

with synthizer.initialized(
    log_level=synthizer.LogLevel.DEBUG, logging_backend=synthizer.LoggingBackend.STDERR
):
    ctx = synthizer.Context()
    noise = synthizer.NoiseGenerator(ctx, channels=2)
    source = synthizer.DirectSource(ctx)
    source.add_generator(noise)

    for t in itertools.cycle(noise_types):
        print("Nois type", t.name)
        noise.noise_type = t
        time.sleep(5.0)
