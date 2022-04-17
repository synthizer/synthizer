include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")
include("./delay_line.jl")
include("./hrtf_cleaner.jl")

import PortAudio, WAV, FileIO, DSP

function build_hrir(dataset::HrtfDataset.Dataset, az, elev)
    elev_lower = nothing
    elev_upper = nothing
    for i in dataset.elevations
        if i.elevation > elev
            elev_upper = i
            break
        end

        elev_lower = i
    end

    if elev_upper === nothing
        elev_upper = elev_lower
    end

    if elev_lower.elevation > elev
        elev = elev_lower.elevation
    end

    function find_azs(e)
        ind = 1
        for i = 1:length(e.azimuths)
            if e.azimuths[i].azimuth > az
                break
            end
            ind = i
        end

        next_ind = mod1(ind + 1, length(e.azimuths))

        (e.azimuths[ind], e.azimuths[next_ind])
    end

    azs_lower = find_azs(elev_lower)
    azs_upper = find_azs(elev_upper)

    function crossfade(azs)
        last_angle = azs[1].azimuth
        next_angle = azs[2].azimuth
        if next_angle < last_angle # this pair wrapped.
            next_angle += 360
        end

        width = next_angle - last_angle
        last_delta = az - last_angle
        @assert last_delta >= 0
        w1 = 1.0 - (last_delta / width)
        w2 = 1.0 - w1
        azs[1].data .* w1 .+ azs[2].data .* w2
    end

    lower_part = crossfade(azs_lower)
    upper_part = crossfade(azs_upper)

    elev = clamp(elev, elev_lower.elevation, elev_upper.elevation)
    height = elev_upper.elevation - elev_lower.elevation
    lower_delta = elev - elev_lower.elevation
    w1 = 1.0 - lower_delta / height
    w2 = 1.0 - w1
    lower_part .* w1 .+ upper_part .* w2
end

file = FileIO.load(ARGS[1])
data = dropdims(sum(file[1], dims = 2), dims = 2) ./ size(file[1])[2]

dataset = HrtfDataset.load_mit("$(@__DIR__)/../mit_kemar")
dataset = HrtfCleaner.clean_dataset(dataset)

left_hrir = build_hrir(dataset, 45, 70)
right_hrir = build_hrir(dataset, 360 - 45, 70)

left = DSP.conv(left_hrir, data)
right = DSP.conv(right_hrir, data)

stream = PortAudio.PortAudioStream(0, 2)
PortAudio.write(stream, [left;; right])
