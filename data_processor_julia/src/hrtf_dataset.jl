"""
Hrtf datasset loader/standard representation.

This module uses degrees, since that's what most HRTF datasets use.

Azimuth is clockwise from front. Elevation is -90=down, 90=up.

It is generally assumed that any HRTF dataset will have fixed elevations and equally spaced azimuths at each elevation.
Even though we can handle something more general in this module, 99% of HRTF datasets take this form and so we don't try
to deal with it currently.

We also assume 44100HZ as the sampling rate; this makes a whole variety of things much easier.

The dataset itself can contain any type of data.  We do this so that we can account for a variety of representations:
HRIR, frequency responses, etc.
"""
module HrtfDataset
import FileIO, WAV

struct Datapoint
  azimuth::Float64
  elevation::Float64
  data
end

struct Elevation
  elevation::Float64
  azimuths::Vector{Datapoint}
end

struct Dataset
  elevations::Vector{Elevation}
  length::Int64
end

mutable struct DatasetBuilder
  points::Vector{Datapoint}
end

DatasetBuilder() = DatasetBuilder([])

function add_point(builder::DatasetBuilder, elevation::Float64, azimuth::Float64, data)
  for p in builder.points
    if elevation == p.elevation && azimuth == p.azimuth
      throw("Attempt to add a duplicate datapoint")
    end
  end

  push!(builder.points, Datapoint(azimuth, elevation, data))
end

function compile(builder::DatasetBuilder)::Dataset
  # Group our points by elevation, then return the struct.
  sort!(builder.points, by=x -> x.elevation)
  elevs::Vector{Elevation} = []
  points::Vector{Datapoint} = []
  cur_angle = builder.points[1].elevation
  l = length(builder.points)
  for i in builder.points
    if i.elevation != cur_angle
      # New elevation starts.
      push!(elevs, Elevation(cur_angle, points))
      points = []
      cur_angle = i.elevation
    end
    push!(points, Datapoint(i.azimuth, i.elevation, i.data))
  end

  push!(elevs, Elevation(cur_angle, points))

  Dataset(elevs, l)
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

function Base.length(ds::Dataset)::Int64
  ds.length
end

"""
The prev/next azimuths  are what's counterclockwise/clockwise of the current elevation and are always set if the current
elevation has more than one point.  The prev/next elevations are what's down/up.  They're none if there is no such
elevation (prev is nothing at the bottom, next nothing at the top).
"""
struct DatasetIteratorItem
  current_azimuth::Float64
  prev_azimuth::Union{Float64,Nothing}
  next_azimuth::Union{Float64,Nothing}
  current_elevation::Float64
  prev_elevation::Union{Float64,Nothing}
  next_elevation::Union{Float64,Nothing}
  data
end

mutable struct DatasetIterator
  elev_index::Int64
  az_index::Int64
  done::Bool
end

function Base.iterate(dataset::Dataset, state::Union{Nothing,DatasetIterator}=nothing)::Union{Nothing,Tuple{DatasetIteratorItem,DatasetIterator}}
  if state === nothing
    if length(dataset.elevations) == 0 || length(dataset.elevations[1].azimuths) == 0
      return nothing
    end

    state = DatasetIterator(1, 1, false)
  end
  if state.done
    return nothing
  end

  elev = dataset.elevations[state.elev_index]
  az = elev.azimuths[state.az_index]

  current_elevation = elev.elevation
  current_azimuth = az.azimuth
  prev_elevation = nothing
  next_elevation = nothing
  prev_azimuth = elev.azimuths[mod1(state.az_index - 1, length(elev.azimuths))].azimuth
  next_azimuth = elev.azimuths[mod1(state.az_index + 1, length(elev.azimuths))].azimuth
  data = az.data

  if state.elev_index > 1
    prev_elevation = dataset.elevations[state.elev_index-1].elevation
    prev_elevation = dataset.elevations[state.elev_index-1].elevation
  end
  if state.elev_index < length(dataset.elevations)
    next_elevation = dataset.elevations[state.elev_index+1].elevation
  end

  state.az_index += 1
  if state.az_index > length(elev.azimuths)
    state.elev_index += 1
    state.az_index = 1
  end

  if state.elev_index > length(dataset.elevations)
    state.done = true
  end

  (DatasetIteratorItem(current_azimuth, prev_azimuth, next_azimuth,
      current_elevation, prev_elevation, next_elevation,
      data), state)
end


"""
Map a function over the elements in this dataset and build a new one.

This is a convenience method for building one up yourself while iterating.
"""
function map_to_dataset(func, dataset::Dataset)
  builder = DatasetBuilder()
  for p in dataset
    data = func(p)
    add_point(builder, p.current_elevation, p.current_azimuth, data)
  end

  compile(builder)
end

end