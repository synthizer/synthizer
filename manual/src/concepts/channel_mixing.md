# Channel Upmixing and Downmixing 

Synthizer has built-in understanding of mono (1 channel) and stereo (2 channels)
audio formats.  It will mix other formats to these as necessary.  Specifically,
we:

- If converting from mono to any other format, broadcast the mono channel to all
  of those in the other format.
- If going to mono, sum and normalize the channels in the other format.
- Otherwise, either drop extra channels or fill extra channels with silence.

Synthizer will be extended to support surround sound in future, which will give
it a proper understanding of 4, 6, and 8 channels.  Since Synthizer is aimed at
non-experimental home media applications, we assume that the channel count is
sufficient to know what the format is goping to be.  For example, there is no
real alternative to 5.1 audio in the home environment if the audio has 6
channels.  If you need more complex multichannel handling, you can pre-convert
your audio to something Synthizer understands.  Otherwise, other libraries may
be a better option.
