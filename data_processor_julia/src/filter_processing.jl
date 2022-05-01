"""
This module handles processing individual impulse responses.

We compute the data to feed into it elsewhere, then call it with that data.

Generally we assume the HRIRs are of the same length.
"""
module FilterProcessing
import FFTW, DSP

"""
Compute the magnitude response of a filter, padded out to match the samplerate, so that we have a resolution of 1HZ.

This also includes the "mirrored" half.  You get [dc, 1, ..., 2, 1].  This is the format expected by minimum_phase.
"""
function compute_magnitude_response(
    impulse::Vector{Float64},
    sr::Int32 = Int32(44100),
)::Vector{Float64}
    padded = impulse
    if length(impulse) < sr
        remaining = sr - length(impulse)
        padded = cat(impulse, zeros(remaining), dims = 1)
    end
    abs.(FFTW.fft(padded))
end

"""
Compute an array which will equalize the power of the passed-in HRIRs when multiplied by the magnitude response.

Hrirs should be an iterator over a set of HRIRs.  This assumes that all of the HRIRs are of the same length.
"""
function compute_power_equalizer(hrirs)::Vector{Float64}
    az_count = 0
    average_power_response = sum(map(hrirs) do h
        az_count += 1
        h .^ 2
    end)
    average_power_response = sqrt.(average_power_response ./ az_count)

    # The following logic stops the response from doing insane things, and was roughly borrowed from the matlab scripts
    # with the MIT Kemar dataset, modified for our use case by listening tests.
    avg_power_db = 20 .* log10.(average_power_response)
    db_range = 20
    avg_power_db = clamp.(avg_power_db, -db_range, db_range)
    offset = sum(avg_power_db) / length(avg_power_db)
    avg_power_db = avg_power_db .- offset

    1.0 ./ (10 .^ (avg_power_db / 20))
end

"""
Compute the right half of a periodic Blackman-harris window, such that it always starts with 1, and ends with an
implicit 0.  We use this for truncation at l points.

The DSP package doesn't have this for some reason.
"""
function half_blackman_harris(l)
    a0 = 0.35875
    a1 = 0.48829
    a2 = 0.14128
    a3 = 0.01168

    # Evaluate the function from 0.5 to 1, excluding 1 
    ns = [0:l-1;]
    mul = 0.5 .+ 0.5 .* ns ./ l
    mul .*= 2 * π

    a0 .- a1 .* cos.(mul) + a2 .* cos.(mul .* 2) - a3 .* cos.(3 .* mul)
end

"""
Truncate a given impulse to a given length by windowing with blackman-harris, then re-normalizing the power.

The window is picked because it matches what WDL does in its resampler truncation.
"""
function truncate_impulse(impulse::Vector{Float64}, length::Int64)::Vector{Float64}
    truncated = impulse[1:length] .* half_blackman_harris(length)
    original_power = sqrt(sum(impulse .^ 2))
    new_power = sqrt(sum(truncated .^ 2))
    power_ratio = original_power / new_power
    mul = sqrt(power_ratio)
    truncated .* mul
end

"""
Given a set of impulse responses, compute the maximum dc.

This works because the zeroth term of a fourier transform is just a sum.
"""
function compute_max_dc(impulses)::Float64
    maximum(map(sum, impulses))
end

"""
Remove the base from an impulse by filtering with a butterworth filter.
"""
function remove_dc(impulse::Vector{Float64}; sr = 44100)::Vector{Float64}
    resp = DSP.Filters.Highpass(100.0; fs = Float64(sr))
    filt = DSP.Filters.digitalfilter(resp, DSP.Filters.Butterworth(2))
    DSP.filt(filt, impulse)
end

"""
Design a filter to emphasize sounds behind the listener, given an azimuth in degrees.
"""
function design_behind_emphasis_filter(azimuth, sr)
    lp_freq = 2000
    lp_stopfreq = sr / 2.0
    # Maximum drop-off in the stopband when the source is straight behind.
    lp_stopband_db = -1.5
    percent = azimuth / 360.0

    # Flip things around so that straight behind is 0, 1 is straight in front, and the value is always positive.  We can
    # use this as a scale factor to our filter design.
    back_relative = 2 * abs(0.5 - percent)

    # We need to leave a little bit of slack here because there are impulses directly to the side, and we don't want to
    # emphasize those.
    if back_relative > 0.49
        # The source is in front; return the identity filter.
        return DSP.Filters.Biquad(1.0, 0.0, 0.0, 0.0, 0.0)
    end

    # Scale factor for the stopband.
    scale_factor = 1.0 - back_relative / 0.5
    stopband = scale_factor * lp_stopband_db

    DSP.Filters.remez(
        16,
        [(0.0, lp_freq - 1) => 1.0, (lp_freq, lp_stopfreq - 1) => 10^(stopband / 20.0)];
        Hz = sr,
    )
end

end
