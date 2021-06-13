"""Demonstrates how to make a buffer from an array of floats by generating binaural beats."""
import array
import math
import sys

import synthizer

SR = 44100
FRAMES = SR * 2
freq_l = 300
freq_r = 302


def gen_sine(output, sr, frequency, length, offset, stride):
    for i in range(length):
        output[i * stride + offset] = math.sin(2 * math.pi * i * frequency / sr)


with synthizer.initialized():
    ctx = synthizer.Context()
    src = synthizer.DirectSource(ctx)
    gen = synthizer.BufferGenerator(ctx)
    src.add_generator(gen)
    gen.looping = True

    data = array.array("f", [0.0] * FRAMES * 2)
    gen_sine(data, SR, freq_l, FRAMES, 0, 2)
    gen_sine(data, SR, freq_r, FRAMES, 1, 2)

    buffer = synthizer.Buffer.from_float_array(SR, 2, data)
    gen.buffer = buffer

    print("Press enter to exit")
    input()
