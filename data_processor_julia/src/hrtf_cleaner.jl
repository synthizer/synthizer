"""
Clean an HRTF dataset.

There's really no good name for this.  This module goes from raw data to something actually usable at runtime by
converting to minimum phase and performing a variety of other enhancements to the data.
"""
module HrtfCleaner
import ..HrtfDataset
import ..FilterProcessing
import ..MinimumPhase
import DSP, FFTW

function clean_dataset(
    dataset::HrtfDataset.Dataset;
    sr = 44100,
    final_filter_length = 32,
    # Clamps the db range to keep things sounding more natural, because the HRTF impulses can have very sharp peaks and
    # valleys.
    db_range = 60,
)::HrtfDataset.Dataset
    mag_dataset = HrtfDataset.map_to_dataset(
        x -> FilterProcessing.compute_magnitude_response(x.data),
        dataset,
    )

    equalpower_inverse = FilterProcessing.compute_power_equalizer([
        j.data for i in mag_dataset.elevations for j in i.azimuths
    ])

    power_equalized_dataset =
        HrtfDataset.map_to_dataset(x -> x.data .* equalpower_inverse, mag_dataset)

    db_min = 10^(-db_range / 20)
    db_max = 10^(20 / db_range)

    clamped_dataset = HrtfDataset.map_to_dataset(power_equalized_dataset) do x
        clamp.(x.data, db_min, db_max)
    end

    minphase = HrtfDataset.map_to_dataset(x -> MinimumPhase.minimum_phase(x.data), power_equalized_dataset)

    emphasized = HrtfDataset.map_to_dataset(minphase) do i
        filt = FilterProcessing.design_behind_emphasis_filter(i.current_azimuth, sr)
        DSP.filt(filt, i.data)
    end

    no_dc = HrtfDataset.map_to_dataset(x -> FilterProcessing.remove_dc(x.data), minphase)

    truncated = HrtfDataset.map_to_dataset(
        x -> FilterProcessing.truncate_impulse(x.data, final_filter_length),
        minphase,
    )

    truncated
end

end