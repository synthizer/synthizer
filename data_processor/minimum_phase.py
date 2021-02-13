import numpy as np
import scipy.signal as sig
import math


def minimum_phase(mag):
    """Convert an impulse response to minimum phase.

    This uses the algorithm described here: https://dsp.stackexchange.com/questions/42917/hilbert-transformer-and-minimum-phase?noredirect=1
    """
    size = len(mag)
    half_size = size // 2
    mag = np.maximum(mag, 1e-6)
    logmag = np.log(mag)
    hilb = np.abs(sig.hilbert(logmag))
    filt = mag * math.e ** (-1j * hilb)
    filt[0] = 0
    filt[half_size] = 0
    inv = np.real(np.fft.ifft(filt))
    return inv
