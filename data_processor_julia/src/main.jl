include("./hrtf_dataset.jl")
include("./minimum_phase.jl")
include("./filter_processing.jl")
include("./delay_line.jl")
include("./hrtf_cleaner.jl")

dataset = HrtfDataset.load_mit("$(@__DIR__)/../mit_kemar")
dataset = HrtfCleaner.clean_dataset(dataset)
