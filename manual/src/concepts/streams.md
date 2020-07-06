# Streams

Streams are sources of audio.  They are specified with 3 string parameters:

- protocol: The protocol. At the moment Synthizer only supports `"file"`.
- path: The path. Interpreted ina  protocol-specific manner. For files, the on-disk path relative to the running execuable.
- options: A string encoded as `"key=value&key=value&..."`. Protocol-specific options.

Whenever a Synthizer API function wants to read audio data, it will request these three parameters to indicate where the data comes from.  In future, it will be possible to register your own streams.

The similarity to a URL is intentional.  We don't yet support parsing in that format, but may opt to do so in future.  Nonetheless it is simple for a host program to do that parsing before passing these parameters to Synthizer, and in particular `file://` URLs match the `file` protocol exactly.

Synthizer supports decoding Flac, Wav, and MP3. Ogg is planned but low priority.
