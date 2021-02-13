from collections import namedtuple
import re
import glob
import os.path
import numpy as np
import scipy.io.wavfile as wavfile
import scipy.signal as signal
import math
import paths
from collections import namedtuple

from minimum_phase import minimum_phase

SR = 44100
NYQUIST = SR // 2


def to_coords(f):
    try:
        bn = os.path.basename(f)
        x = re.match("L(.*)e(.*)a.wav", bn)
        az, elev = int(x[2]), int(x[1])
        wav = wavfile.read(f)
        assert wav[0] == 44100
        data = wav[1]
        data = data / 32768.0
        return az, elev, data
    except:
        print(f)
        raise


def load_dataset():
    """Returns the dataset as a dict elev -> azimuth -> f32 array."""
    d = {}
    files = glob.glob(os.path.join(paths.data_path, "elev*", "L*.wav"), recursive=True)
    for f in files:
        az, elev, data = to_coords(f)
        x = d.get(elev, {})
        x[az] = data
        d[elev] = x
    assert len(d) > 2, "Must have at least 2 elevations"
    print(f"Have {len(d)} elevations")
    return d


ElevationMetadata = namedtuple(
    "ElevationMetadata",
    ["elev_angles", "degs_per_elev", "elev_min", "elev_max", "num_elevs"],
)


def extract_elevation_metadata(dataset):
    elev_angles = sorted(dataset.keys())
    degs_per_elev = elev_angles[1] - elev_angles[0]
    print("Checking that elevations are equidistant")
    for i in range(len(elev_angles) - 1):
        assert (
            elev_angles[i + 1] - elev_angles[i] == degs_per_elev
        ), f"Elevation must be equal to {degs_per_elev}"

    elev_min = elev_angles[0]
    elev_max = elev_angles[-1]
    return ElevationMetadata(
        elev_angles=elev_angles,
        degs_per_elev=degs_per_elev,
        elev_min=elev_min,
        elev_max=elev_max,
        num_elevs=len(dataset),
    )


def unfold_azimuths(dataset):
    """Returns a [[azimuths], [azimuths]...] list, where the outer list is per election.

    This is useful for processing, which doesn't have to worry about iterating the dict right."""
    print("Unfolding azimuths")
    azimuths = []
    for a, e in sorted(dataset.items()):
        azs = sorted(e.keys())
        azs = [e[i] for i in azs]
        azimuths.append(azs)
    return azimuths


def map_azimuths(azimuths, fn):
    out = []
    for e in azimuths:
        az = []
        for a in e:
            az.append(fn(a))
        out.append(az)
    return out


def magnitude_response(array):
    # Make sure to really overdo this; we want more than 256 bins.
    # Match the samplerate of the dataset, and each bin is then 1 hz, which is
    # convenient.
    return np.abs(np.fft.fft(array, n=SR))


def convert_to_magnitude_responses(azimuths):
    print("Building magnitude responses")
    return map_azimuths(azimuths, magnitude_response)


def equalize_power(azimuths):
    print("Equalizing power response")
    presp = np.zeros(len(azimuths[0][0]), dtype=np.float64)
    indices = [(i, j) for i in range(len(azimuths)) for j in range(len(azimuths[i]))]
    new_az = [[None] * len(i) for i in azimuths]
    c = sum([len(i) for i in azimuths])
    for i, j in indices:
        presp += azimuths[i][j] ** 2
    average_power = presp / c
    # for frequencies abovethis value, set the average to 1; this allows
    # the dataset to emphasize ahead/behind
    stop_at = 5000
    # We apply this twice, once before averaging and once after. This is because it makes the averaging step easy,
    # since any value set to 1.0 before going to log-magnitude comes out to 0.
    average_power[stop_at : len(average_power) - stop_at] = 1.0

    # Now, we want to limit this filter so that it's not doing insane things. Following logic was roughly borroed
    # from the matlab scripts distributed with the MIT kemar dataset.
    avg_power_log = 20 * np.log10(np.abs(average_power))
    db_range = 20
    # Recall that everything after stop_at is 0.0 db. We don't want to count it.
    offset = sum(avg_power_log) / stop_at
    avg_power_log -= offset
    # avg_power_log = np.minimum(np.maximum(avg_power_log, -db_range), db_range)

    filter = np.sqrt(10 ** (-avg_power_log / 20))
    for i, j in indices:
        new_az[i][j] = azimuths[i][j] * filter
        new_az[i][j][0] = 1.0
    return new_az


def clamp_responses(azimuths):
    print("Clamping responses to be between -60 db and 3 db")
    min_gain = 0.0  # 10**(-40/20)
    max_gain = 10 ** (6 / 20)
    print(f"Min {min_gain} max {max_gain}")

    def normalize(a):
        return np.maximum(np.minimum(a, max_gain), min_gain)

    return map_azimuths(azimuths, normalize)


def conv_minimum_phase(azimuths):
    print("Converting to minimum phase")

    def minphase(a):
        a = np.array(a)
        return minimum_phase(a)

    return map_azimuths(azimuths, minphase)


def truncate_hrir(azimuths, hrir_length):
    print(f"Windowing to {hrir_length} points")
    # We use blackman-harris because the WDL likes it for its resampler, so proceeding under the assumption that it's good enough for us too.
    blackman = signal.blackmanharris(hrir_length * 2 - 1)[-hrir_length:]
    assert len(blackman) == hrir_length
    assert blackman[0] == 1.0

    return map_azimuths(azimuths, lambda a: a[:hrir_length] * blackman)


def compute_dc(azimuths):
    dcs = map_azimuths(azimuths, lambda a: np.sum(a))
    dcs_flat = [j for i in dcs for j in i]
    return max(dcs_flat)


base_butter = signal.butter(N=2, btype="highpass", fs=SR, Wn=100)


def run_base_filter(a):
    return signal.lfilter(b=base_butter[0], a=base_butter[1], x=a)


def remove_base(azimuths):
    return map_azimuths(azimuths, run_base_filter)


def emphasize_behind(azimuths):
    """Adds a lowpass filter which emphasises when sounds are behind the listener. The values here were determined through listening to it,
    and have no real logic beyond that"""
    lp_freq = 2000  # frequency for the lowpass stopband's start.
    lp_stopfreq = NYQUIST  # The frequency that the filter's stopband fully starts at.
    lp_stopband_db = (
        -1.5
    )  # maximum rolloff for the stopband, when the source is straight behind.

    print("Emphasizing sources behind the listener with a lowpass filter")

    az_out = [[None] * len(i) for i in azimuths]
    for i, elev in enumerate(azimuths):
        for j, entry in enumerate(elev):
            # find percent around the circle
            percent = j / len(elev)
            # back_relative is 0 for straight behind, 0.5 for the side, 1 for straight in front.
            # It doesn't encode which side.
            back_relative = 2 * abs(0.5 - percent)
            # Leave a little bit of slack in this if statement for floating point error.
            if back_relative >= 0.49:
                # It's in front; don't do anything.
                az_out[i][j] = np.array(entry)
                continue
            # Convert to a scale factor for the stopband: 1.0 for straight to the side, 0.0 for behind.
            # The exponent here is to make it less abrupt at the side transition points
            scale_factor = 1.0 - back_relative / 0.5
            # scale_factor = scale_factor**2
            # Then, take that to astopband
            # Note that numpy will choke if this is too low.
            stopband_gain = -0.1 + scale_factor * lp_stopband_db
            # Now design and run the filter:
            # Note that gstop is actually positive, so we have to flip it.
            b, a = signal.iirdesign(
                wp=lp_freq,
                ws=lp_stopfreq,
                fs=SR,
                gpass=0.1,
                gstop=-stopband_gain,
                ftype="butterworth",
            )
            filtered = signal.lfilter(b, a, entry)
            az_out[i][j] = filtered
    return az_out


# this is the data that we need to write out.
HrtfData = namedtuple(
    "HrtfData",
    [
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
    ],
)


def compute_hrtf_data():
    hrir_length_final = 32
    np.seterr(all="raise")
    dataset = load_dataset()
    elev_meta = extract_elevation_metadata(dataset)
    azimuths = unfold_azimuths(dataset)
    print("Initial dc", compute_dc(azimuths))
    azimuths = convert_to_magnitude_responses(azimuths)
    azimuths = equalize_power(azimuths)
    azimuths = clamp_responses(azimuths)
    azimuths = conv_minimum_phase(azimuths)
    print("Minimum phase dc", compute_dc(azimuths))
    azimuths = remove_base(azimuths)
    azimuths = truncate_hrir(azimuths, hrir_length_final)
    print("Truncated dc", compute_dc(azimuths))
    azimuths = emphasize_behind(azimuths)

    num_azs = [len(i) for i in azimuths]
    impulse_length = len(azimuths[0][0])
    assert impulse_length == hrir_length_final

    ret = HrtfData(
        num_elevs=elev_meta.num_elevs,
        elev_min=elev_meta.elev_min,
        elev_increment=elev_meta.degs_per_elev,
        num_azimuths=num_azs,
        azimuths=azimuths,
        impulse_length=impulse_length,
    )
    print(ret._replace(azimuths=None))
    print(azimuths[0][0])
    return ret
