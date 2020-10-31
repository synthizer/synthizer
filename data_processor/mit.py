from collections import namedtuple
import re
import glob
import os.path
import numpy as np
import scipy.io.wavfile as wavfile
import scipy.signal as signal
import math
import paths
from minimum_phase import minimum_phase

files = glob.glob(os.path.join(paths.data_path, "elev*", "L*.wav"), recursive=True)

def to_coords(f):
    try:
        bn = os.path.basename(f)
        x = re.match('L(.*)e(.*)a.wav', bn)
        az, elev = int(x[2]), int(x[1])
        wav = wavfile.read(f)
        assert wav[0] == 44100
        data = wav[1];
        data = data / 32768.0
        return az, elev, wav[1]
    except:
        print(f)
        raise

d = {}
for f in files:
    az, elev, data = to_coords(f)
    x = d.get(elev, {})
    x[az] = data
    d[elev] = x

assert len(d) > 2, "Must have at least 2 elevations"
print(f"Have {len(d)} elevations")

elev_angles = sorted(d.keys())
degs_per_elev = elev_angles[1]-elev_angles[0]

print("Checking that elevations are equidistant")
for i in range(len(elev_angles)-1):
    assert elev_angles[i+1]-elev_angles[i] == degs_per_elev, f"Elevation must be equal to {degs_per_elev}"

elev_min = elev_angles[0]
elev_max = elev_angles[-1]

print("Unfolding azimuths")
azimuths = []
for e in elev_angles:
    azs = sorted(d[e].keys())
    azs = [d[e][i] for i in azs]
    azimuths.append(azs)

indices = [(i, j) for i in range(len(azimuths)) for j in range(len(azimuths[i]))]

print("Building magnitude responses")
for i, j in indices:
    azimuths[i][j] = np.abs(np.fft.fft(azimuths[i][j]))

print("Equalizing power response")
presp = np.zeros(len(azimuths[0][0]), dtype=np.float64)
c = 0
for i, j in indices:
    c += 1
    presp += azimuths[i][j]**2
average_power = presp/c
for i, j in indices:
    azimuths[i][j] = np.sqrt(azimuths[i][j]**2 / average_power)
    azimuths[i][j][0] = 1.0

print("Clamping responses to be between -60 db and 3 db")
min_gain = 10**(-60/20)
max_gain = 10**(3/20)
print(f"Min {min_gain} max {max_gain}")
for i, j in indices:
    azimuths[i][j] = np.maximum(np.minimum(azimuths[i][j], max_gain), min_gain)

print("Converting to minimum phase")
for i, j in indices:
    azimuths[i][j] = minimum_phase(azimuths[i][j])

hrir_length_final = 32
print(f"Windowing to {hrir_length_final} points")
# We use blackman-harris because the WDL likes it for its resampler, so proceeding under the assumption that it's good enough for us too.
blackman = signal.blackmanharris(hrir_length_final*2-1)[-hrir_length_final:]
assert len(blackman) == hrir_length_final
assert blackman[0] == 1.0

for i, j in indices:
    azimuths[i][j] = azimuths[i][j][:hrir_length_final]*blackman

# this is the data that we need to write out.
HrtfData = namedtuple("HrtfData", [
    # Number of elevations in the dataset.
    "num_elevs",
    # Increment of the elevation in degrees.
    "elev_increment",
    # Min elevation angle in degrees.
    "elev_min",
    # num_elevs-length list.
    # Holds the azimuth count for each elevation.
    # For now, we assume azimuths are equally distributed.
    "num_azimuths",
    # The azimuths themselves as an array of arrays of arrays.
    "azimuths",
    # Number of data points in the set.
    "impulse_length",
])

num_elevs = len(azimuths)
elev_min = min(d.keys())
tmp = sorted(d.keys())
elev_increment = tmp[1]-tmp[0]
num_azs = [len(i) for i in azimuths]
impulse_length = len(azimuths[0][0])
assert impulse_length == hrir_length_final

hrtf_data = HrtfData(
    num_elevs = num_elevs,
    elev_min = elev_min,
    elev_increment = elev_increment,
    num_azimuths = num_azs,
    azimuths = azimuths,
    impulse_length = impulse_length
)

