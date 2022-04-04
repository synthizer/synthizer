include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")

dataset = HrtfDataset.load_mit("$(@__DIR__)/../mit_kemar")
dataset = HrtfDataset.map_to_dataset(dataset) do x
    x
end
