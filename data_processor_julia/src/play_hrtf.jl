module PlayHrtf
import PortAudio, WAV, FileIO, DSP
import ..HrtfDataset, ..DelayLine

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
        @assert last_delta >= 0 "last_angle=$(last_angle), az=$(az), next_angle=$(next_angle)"
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

function conv_del(line, impulse, delay)
    sum = 0.0
    for i = 1:length(impulse)
        sample = DelayLine.read(line, UInt64(delay + i - 1))[1]
        sum += sample * impulse[i]
    end
    sum
end

function do_hrir(line, left_del, left_impulse, right_del, right_impulse)
    left_del_floor = floor(left_del)
    left_del_ceil = ceil(left_del)
    right_del_floor = floor(right_del)
    right_del_ceil = ceil(right_del)

    left_floor = conv_del(line, left_impulse, left_del_floor)
    left_ceil = conv_del(line, left_impulse, left_del_ceil)
    right_floor = conv_del(line, right_impulse, right_del_floor)
    right_ceil = conv_del(line, right_impulse, right_del_ceil)

    lw2 = left_del - left_del_floor
    lw1 = 1.0 - lw2
    rw2 = right_del - right_del_floor
    rw1 = 1.0 - rw2

    return (left_floor * lw1 + left_ceil * lw2, right_floor * rw1 + right_ceil * rw2)
end

function tick_hrir(line, input_sample, left_del, left_impulse, right_del, right_impulse)
    DelayLine.write(line, (input_sample,))
    return do_hrir(line, left_del, left_impulse, right_del, right_impulse)
end

# Returns (left_delay, right_delay)
function compute_delays(azimuth, elevation; sr = 44100)
    az_r = azimuth * π / 180.0
    elev_r = elevation * π / 180.0

    # Convert our direction to a cartesian vector, keeping x.
    #
    # az_r is the clockwise angle from y, which is forward. cos(az_r) is the y component. Consequently, sin(az_r) is the
    # x component.
    #
    # This is the standard spherical coordinate conversion, but adjusted to account for our flipped coordinate system
    # and dropping all the components we don't need.
    x = sin(az_r) * cos(elev_r)

    # The angle between y and our spherical vector, in the range 0, pi/2, which "pretends" the source is in the front
    # right quadrant.  Note that the front right and back right yield the same itd, and the front left and back left
    # yield the same itd as well, but flipped.
    #
    # We have the angle between our vector and x, because x=cos(theta) in this instance: the vector we're getting an
    # angle with is a unit vector in the unit circle, so this is true by definition.
    angle = π / 2 - acos(abs(x))

    # ITD in seconds using woodworth.
    head_rad = 0.15
    sos = 343.0
    itd = (head_rad / sos) * (angle + sin(angle))

    # Get the side of the head the lower itd is on.  This works because 0 to π is the right half, as is 2π to 3πand so
    # on.  This pattern means we take whether or not the division is even or odd as our answer; even is right, odd is
    # left.
    pi_intervals = Int32(floor(az_r / π))
    even = pi_intervals ÷ 2 == 0

    if even
        return (sr * itd, 0.0)
    else
        return (0.0, sr * itd)
    end
end

# Load the specified file, and repeat it until at least samples worth of data are available.
function load_file(file, samples)
    file = FileIO.load(file)
    data = dropdims(sum(file[1], dims = 2), dims = 2) ./ size(file[1])[2]

    reps = samples / length(data)
    reps += 1

    data = repeat(data, UInt32(floor(reps)))

    return data
end

function play_hrtf(dataset, file)
    samples = 44100 * 10
    az = 0
    elev = 0
    step_len = 1000
    increment = 1

    line = DelayLine.Line(Val{1}(), 44100)

    data = load_file(file, samples)

    left_hrir = nothing
    right_hrir = nothing
    itd_l = nothing
    itd_r = nothing

    res_l = []
    res_r = []

    last_computed_step = -1
    for i = 1:samples
        step = mod1(i, step_len)
        if step != last_computed_step
            last_computed_step = step
            effective_az = mod(az + i / step_len * increment, 360)
            left_hrir = build_hrir(dataset, effective_az, elev)
            right_hrir = build_hrir(dataset, 360 - effective_az, elev)
            (itd_l, itd_r) = compute_delays(effective_az, elev)
        end

        (l, r) = tick_hrir(line, data[i], itd_l, left_hrir, itd_r, right_hrir)
        push!(res_l, l)
        push!(res_r, r)
    end

    stream = PortAudio.PortAudioStream(0, 2)
    PortAudio.write(stream, [res_l;; res_r])
end

end