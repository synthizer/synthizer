# Loading Libsndfile

Synthizer supports 3 built-in audio formats: wav, MP3, and Flac.  For apps which
need more, Synthizer supports loading
[Libsndfile](http://www.mega-nerd.com/libsndfile/).  To do so, use
`syz_initializeWithConfig` and configure `libsndfile_path` to be the absolute
path to a Libsnddfile shared object (`.dll`, `.so`, etc).  Libsndfile will then
automatically be used where possible, replacing the built-in decoders.

Unfortunately, due to Libsndfile limitations, Libsndfile can only be used on
seekable streams of known length.  All Synthizer-provided methods of decoding
currently support this, but custom streams may opt not to do so, for example if
they're reading from the network.  In this case, Libsndfile will be skipped. To
see if this is happening, enable debug logging at library initialization and
Synthizer will log what decoders it's trying to use.

Because of licensing incompatibilities, Libsndfile cannot be statically linked
with Synthizer without effectively changing Synthizer's license to LGPL.
Consequently dynamic linking with explicit configuration is the only way to use
it.  Your app will need to arrange to distribute a Libsndfile binary as well and
use the procedure described above to load it.