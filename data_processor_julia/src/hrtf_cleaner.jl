"""
Clean an HRTF dataset.

There's really no good name for this.  This module goes from raw data to something actually usable at runtime by
converting to minimum phase and performing a variety of other enhancements to the data.
"""
module HrtfCleaner
import ..HrtfDataset
import ..FilterProcessing
import ..MinimumPhase
import DSP

function clean_dataset(dataset; sr = 44100, final_filter_length = 32)
    mag_dataset = HrtfDataset.map_to_dataset(
        x -> FilterProcessing.compute_magnitude_response(x.data),
        dataset,
    )

    equalpower_inverse = FilterProcessing.compute_power_equalizer([
        j.data for i in mag_dataset.elevations for j in i.azimuths
    ])

    power_equalized_dataset =
        HrtfDataset.map_to_dataset(x -> x.data .* equalpower_inverse, mag_dataset)

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

    truncated
end

end