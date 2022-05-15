module MinimumPhase

import FFTW, DSP

# Screen readers don't like \euler.
E = ℯ

"""
Convert an impulse response to minimum phase.

This uses the algorithm described here:
https://dsp.stackexchange.com/questions/42917/hilbert-transformer-and-minimum-phase?noredirect=1
"""
function minimum_phase(amplitude_response::Vector{Float64})::Vector{Float64}
    # We can't deal with zeros in the amplitude response because of the logarithm and this is an audio application which
    # needn't be mathematically perfect, so add a little bit 
    safe_amplitude_response = max.(amplitude_response, 1e-20)
    log_amplitude_response = log.(safe_amplitude_response)

    # The DSP package is giving us the hilbert transform in the imaginary components and leaving our input in the reals.
    # That is, it is assuming we are trying to compute an analytic signal.
    #
    # But we aren't, we need the actual raw hilbert transform, so pluck that back out.
    hilb = imag.(DSP.hilbert(log_amplitude_response))

    minphase_freq = safe_amplitude_response .* E .^ (im .* hilb)

    real.(FFTW.ifft(minphase_freq))
end

end
