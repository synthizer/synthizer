include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")
include("./delay_line.jl")

import DSP

sr = 44100
final_filter_length = 32

dataset = HrtfDataset.load_mit("$(@__DIR__)/../mit_kemar")

mag_dataset =
    HrtfDataset.map_to_dataset(x -> FilterProcessing.compute_magnitude_response(x.data), dataset)

equalpower_inverse = FilterProcessing.compute_power_equalizer([
    j.data for i in mag_dataset.elevations for j in i.azimuths
])
println(equalpower_inverse[1:5])
power_equalized_dataset = HrtfDataset.map_to_dataset(x -> x.data .* equalpower_inverse, mag_dataset)
println(power_equalized_dataset.elevations[1].azimuths[1].data[1:5])

minphase =
    HrtfDataset.map_to_dataset(x -> MinimumPhase.minimum_phase(x.data), power_equalized_dataset)

emphasized = HrtfDataset.map_to_dataset(minphase) do i
    filt = FilterProcessing.design_behind_emphasis_filter(i.current_azimuth, sr)
    DSP.filt(filt, i.data)
end

no_dc = HrtfDataset.map_to_dataset(x -> FilterProcessing.remove_dc(x.data), emphasized)

truncated = HrtfDataset.map_to_dataset(
    x -> FilterProcessing.truncate_impulse(x.data, final_filter_length),
    no_dc,
)
println(truncated.elevations[1].azimuths[1].data)
