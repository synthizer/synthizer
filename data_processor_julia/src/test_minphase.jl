# This script prints out a bunch of useful numbers to verify that the minimum phase conversion works.  it now does, but
# keeping this for posterity/possible future need.
include("./minimum_phase.jl")

import DSP, FFTW

input = [1, -1, 2, 3, -4, 5]
input_p = sum(input .^ 2)

input_mag = abs.(FFTW.fft(input))
input_phase = angle.(FFTW.fft(input)) * 180 / π

output = MinimumPhase.minimum_phase(input_mag)

output_mag = abs.(FFTW.fft(output))
output_phase = angle.(FFTW.fft(output)) .* 180 / π

println(input_p)
println(input_mag)
println(input_phase)
println(sum(output .^ 2))
println(output_mag)
println(output_phase)
println("")
println(output)
