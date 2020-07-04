# Channel Upmixing and Downmixing 


Synthizer's current channel mixing algorithm is as follows:

- Mono to anything duplicates in all channels.
- Anything to mono sums all channels and divides.
- Otherwise, missing channels are zero-initialized and extra channels are dropped.

Though this algorithm will be extended in future, note that Synthizer is for games and VR applications, and that it is usually impossible to determine channel layout from media files with 100% reliability.
When better support is added, this page wil be extended explaining how it works, but expect to need to perform media type conversions or to add stream options if working with more than 2 channels per asset.