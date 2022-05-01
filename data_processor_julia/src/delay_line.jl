"""
A simple delay line, supporting adding samples and reading into the past.
"""
module DelayLine

mutable struct Line{Channels}
    data::Matrix{Float64}
    pos::UInt64

    function Line(::Val{Channels}, length)::DelayLine{Channels} where {Channels}
        data = zeros(Float64, Channels, length)
        new{Channels}(data, UInt64(1))
    end
end

"""
Read this delay line, at relative_pos samples in the past.

The default value for relative_pos, 1, means that a write call followed by a read call will read the value of the write
call because the write call just advanced.
"""
function read(
    line::Line{Channels},
    relative_pos::UInt64 = UInt64(1),
)::NTuple{Channels, Float64} where {Channels}
    pos = convert(Int64, line.pos) - convert(Int64, relative_pos)
    pos = mod1(pos, size(line.data)[2])

    @assert 1 <= pos <= size(line.data)[2]
    ntuple(Val(Channels)) do i
        line.data[i, pos]
    end
end

"""
Write this delay line.  Always at the current position.  Advances the position by one.
"""
function write(line::Line{Channels}, vals::NTuple{Channels, Float64}) where {Channels}
    # Must be first, otherwise we don't point at the latest write and all reads which want to read the current sample
    # get nothing until the line wraps.
    line.pos = mod1(line.pos + 1, size(line.data)[2])
    for i = 1:Channels
        line.data[i, line.pos] = vals[i]
    end
end

end
