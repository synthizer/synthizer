# Streams

Streams are sources of audio.  They are specified with 2 string parameters and an opaque void parameter:

- protocol: The protocol. At the moment Synthizer only supports `"file"`.
- path: The path. Interpreted ina  protocol-specific manner. For files, the on-disk path relative to the running executable.
- param: an arbitrary parameter for user-defined streams.  Should always be NULL for built-in Synthizer streams.

Whenever a Synthizer API function wants to read audio data, it will request these three parameters to indicate where the data comes from.  In future, it will be possible to register your own streams.

Synthizer supports decoding Flac, Wav, and MP3. Ogg is planned but low priority.
