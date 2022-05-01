include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")
include("./delay_line.jl")
include("./hrtf_cleaner.jl")
include("./synthesize_hrtf_dataset.jl")
include("./play_hrtf.jl")

import Serialization

cmd = ARGS[1]
if cmd == "build"
    dataset = SynthesizeHrtfDataset.synthesize_hrtf_dataset()
    Serialization.serialize("hrtf.db", dataset)
    println(sum(dataset.elevations[5].azimuths[1].data .^ 2))
    println(sum(dataset.elevations[5].azimuths[37].data .^ 2))
elseif cmd == "play"
    dataset = Serialization.deserialize("hrtf.db")
    PlayHrtf.play_hrtf(dataset, ARGS[2])
end
