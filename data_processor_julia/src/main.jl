include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")
include("./delay_line.jl")
import .DelayLine as d

line = d.Line(Val{2}(), UInt64(4))

for i = 1:4
    d.write(line, (Float64(i), Float64(i * 2)))
    print(d.read(line, UInt64(2)))
end
