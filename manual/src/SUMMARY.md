# Summary

- [Introduction](./introduction.md)

# Tutorials

- [A Basic Python Tutorial](./tutorials/python.md)
- [Events (alpha)](./tutorials/events.md)
- [Filters in Python](./tutorials/python_filters.md)

# Concepts

- [Introduction to the C API](./concepts/c_api.md)
- [Logging, Initialization, and Shutdown](./concepts/initialization.md)
- [Handles and userdata](./concepts/handles.md)
- [Basics of Audio In Synthizer](./concepts/audio_in.md)
  - [The Context](./concepts/context.md)
  - [Introduction to Generators](./concepts/generators.md)
  - [Introduction to Sources](./concepts/sources.md)
  - [Controlling Object Properties](./concepts/properties.md)
  - [Setting Gain/volume](./concepts/gain.md)
  - [Pausing and Resuming Playback](./concepts/pausing.md)
  - [Configuring Objects to Continue Playing Until Silent](./concepts/lingering.md)
  - [Streams and Decoding Audio Data](./concepts/decoding.md)
  - [Loading Libsndfile](./concepts/libsndfile.md)
  - [Implementing Custom Streams and Custom Stream Protocols](./concepts/custom_streams.md)
  - [Channel Upmixing and Downmixing ](./concepts/channel_mixing.md)
- [ 3D Audio, Panning, and HRTF](./concepts/3d_audio.md)
- [Filters and Effects](./concepts/filters_and_effects.md)
  - [Filters](./concepts/filters.md)
  - [Effects and Effect Routing](./concepts/effects.md)
- [Stability Guarantees](./concepts/stability.md)

# The Object Reference

- [Context](./object_reference/context.md)
- [Buffer](./object_reference/buffer.md)

---

- [Operations Common to All Sources](./object_reference/source.md)
- [DirectSource](./object_reference/direct_source.md)
- [Operations Common to Panned and 3D Sources](./object_reference/spatialized_source.md)
- [PannedSource](./object_reference/panned_source.md)
- [Source3D](./object_reference/source_3d.md)

---

- [Operations Common to All Generators](./object_reference/generator.md)
- [StreamingGenerator](./object_reference/streaming_generator.md)
- [BufferGenerator](./object_reference/buffer_generator.md)
- [NoiseGenerator](./object_reference/noise_generator.md)

---

- [Operations Common to All Effects](./object_reference/global_effect.md)
- [Echo](./object_reference/echo.md)
- [FdnReverb](./object_reference/fdn_reverb.md)

# Appendecies

- [Audio EQ Cookbook](./appendices/audio_eq_cookbook.md)

# Release notes

- [0.8.X](./releases/0.8.x.md)
- [0.0 to 0.7.x](./releases/pre-0.8.md)
