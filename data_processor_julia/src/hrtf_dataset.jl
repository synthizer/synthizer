"""
Hrtf datasset loader/standard representation.

This module uses degrees, since that's what most HRTF datasets use.

Azimuth is clockwise from front. Elevation is -90=down, 90=up.

It is generally assumed that any HRTF dataset will have fixed elevations and equally spaced azimuths at each elevation.
Even though we can handle something more general in this module, 99% of HRTF datasets take this form and so we don't try
to deal with it currently.

We also assume 44100HZ as the sampling rate; this makes a whole variety of things much easier.
"""
module HrtfDataset
import FileIO, WAV

struct Datapoint
  azimuth::Float64
  elevation::Float64
  hrir::Vector{Float64}
end

struct Elevation
  elevation::Float64
  azimuths::Vector{Datapoint}
end

struct Dataset
  elevations::Vector{Elevation}
end

mutable struct DatasetBuilder
  points::Vector{Datapoint}
end

DatasetBuilder() = DatasetBuilder([])

function add_point(builder::DatasetBuilder, elevation::Float64, azimuth::Float64, hrir::Vector{Float64})
  for p in builder.points
    if elevation == p.elevation && azimuth == p.azimuth
      throw("Attempt to add a duplicate datapoint")
    end
  end

  push!(builder.points, Datapoint(azimuth, elevation, hrir))
end

function compile(builder::DatasetBuilder)::Dataset
  # Group our points by elevation, then return the struct.
  sort!(builder.points, by=x -> x.elevation)
  elevs::Vector{Elevation} = []
  points::Vector{Datapoint} = []
  cur_angle = builder.points[1].elevation
  for i in builder.points
    if i.elevation != cur_angle
      # New elevation starts.
      push!(elevs, Elevation(cur_angle, points))
      points = []
      cur_angle = i.elevation
    end
    push!(points, Datapoint(i.azimuth, i.elevation, i.hrir))
  end

  push!(elevs, Elevation(cur_angle, points))

  Dataset(elevs)
end

function load_mit(path::String)::Dataset
  dirs = collect(walkdir(path))
  ftuples = [i for i in dirs if length(i[3]) > 0]
  all_files = [joinpath(i[1], j) for i in ftuples for j in i[3]]

  builder = HrtfDataset.DatasetBuilder()

  pattern = r"L(-?\d+)e(\d+)"
  for f in all_files
    res = match(pattern, f)
    if res === nothing
      continue
    end

    elev = parse(Float64, res.captures[1])
    az = parse(Float64, res.captures[2])
    hrir = FileIO.load(f)

    HrtfDataset.add_point(builder, elev, az, vec(hrir[1]))
  end

  dataset = HrtfDataset.compile(builder)
end

end