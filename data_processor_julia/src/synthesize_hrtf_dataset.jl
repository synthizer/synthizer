module SynthesizeHrtfDataset
import ..HrtfDataset, ..HrtfCleaner

function synthesize_hrtf_dataset()
    dataset = HrtfDataset.load_mit("$(@__DIR__)/../mit_kemar")
    return HrtfCleaner.clean_dataset(dataset)
end
end
