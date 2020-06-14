#pragma once

#include "config.hpp"
#include "memory.hpp"
#include "types.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace synthizer {

/*
 * Buffers hold decoded audio data as 16-bit samples resampled to the Synthizer samplerate.  The following entities are involved:
 * 
 * - A buffer holds the data itself.
 * - A BufferRef holds a reference to a buffer.
 * - A BufferChunkRef holds a non-owning reference to a chunk from a buffer.
 * 
 * A BufferReader type is also provided, which provides convenience methods for reading without having to deal with assemblying objects yourself.
 * 
 * Design justifications/explanations:
 * 
 * - We use 16-bit samples because this is enough for audio playback, and it cuts ram usage in half w.r.t. floats.
 * - We store data in noncontiguous chunks in order to assist low memory systems, and also because this makes it harder for someone to just copy audio data out with a debugger,
 *   since doing so would require walking our internal structures. Also, branch prediction should make reading cheap.
 * - We resample first for CPU usage purposes.
 * - A Buffer and BufferRef are separate types because we can expose BufferRefs through the C API, which will allow us to offer a BufferCache in future,
 *   rather than making everyone implement this themselves.
 * 
 * Most of this file is inline because otherwise things get expensive very quickly short of compiling with LTO and optimization and it would be nice to be able to use these in debug builds.
 * */

/* Helper type to hold a pointer and a length for buffer chunks. */
class BufferChunkRef {
	public:
	std::size_t start = 0;
	std::size_t end = 0;
	std::int16_t *data = nullptr;
};

/*
 * A Buffer holds the actual audio data, and is simply a vector of pointers.
 * */
class Buffer {
	public:
	/* The pages are built by a decoder or something else, then fed here. They must be allocated with allocAligned. */
	Buffer(unsigned int channels, std::size_t length, std::vector<std::int16_t*> &&chunks): channels(channels),
	length(length), chunks(std::move(chunks)) {
	}

	~Buffer() {
		for (auto c: this->chunks) {
			freeAligned(c);
		}
	}

	unsigned int getChannels() const {
		return this->channels;
	}

	std::size_t getLength() const {
		return this->length;
	}

	BufferChunkRef getChunk(std::size_t index) const {
		std::size_t actual_index = index / config::BUFFER_CHUNK_SIZE;
		std::size_t rounded_index = actual_index * config::BUFFER_CHUNK_SIZE;
		return {
			.start = rounded_index,
			/* Note: this is more complicated because it has to account for a potential partial page at the end. */
			.end = rounded_index + std::min(this->length - rounded_index, config::BUFFER_CHUNK_SIZE),
			.data = this->chunks[actual_index],
		};
	}

	private:
	unsigned int channels;
	std::size_t length;
	std::vector<std::int16_t*> chunks;
};

class BufferRef: CExposable {
	public:
	BufferRef(const std::shared_ptr<Buffer> &b): buffer(b) {
	}

	unsigned int getChannels() const {
		return this->buffer->getChannels();
	}

	std::size_t getLength() const {
		return this->buffer->getLength();
	}

	BufferChunkRef getChunk(std::size_t index) const {
		return this->buffer->getChunk(index);
	}

	private:
	std::shared_ptr<Buffer> buffer;
};

class BufferReader {
	public:
	BufferReader(const BufferRef &ref): buffer(ref) {
		this->channels = this->buffer.getChannels();
		assert(this->channels < config::MAX_CHANNELS);
	}

	std::int16_t readSampleI16(std::size_t pos, unsigned int channel) {
		assert(channel < this->channels);
		/* Fast case. */
		if (this->chunk.start <= pos && pos < this->chunk.end) {
			goto do_read;
		} else if (pos >= this->buffer.getLength()) {
			return 0;
		} else {
			this->chunk = this->buffer.getChunk(pos);
			goto do_read;
		}
		do_read:
		return this->chunk.data[(pos - this->chunk.start) * this->channels + channel];
	}

	AudioSample readSample(std::size_t pos, unsigned int channel) {
		return this->readSampleI16(pos, channel) / 32768.0f;
	}

	void readFrameI16(std::size_t pos, std::int16_t *out) {
		if (this->chunk.start <= pos && pos < this->chunk.end) {
			goto do_read;
		} else if (pos > this->buffer.getLength()) {
			std::fill(out, out + this->channels, 0);
			return;
		} else {
			this->chunk = this->buffer.getChunk(pos);
			goto do_read;
		}
	do_read:
		std::int16_t *ptr = this->chunk.data + (pos -  this->chunk.start) * this->channels;
		std::copy(ptr, ptr + this->channels, out);
	}

	void readFrame(std::size_t pos, AudioSample *out) {
		std::array<std::int16_t, config::MAX_CHANNELS> intermediate;
		readFrameI16(pos, &intermediate[0]);
		for (unsigned int i = 0; i < this->channels; i++) {
			out[i] = intermediate[i] / 32768.0f;
		}
	}

	/* Returns frames read. This will be less than requested at the end of the buffer. */
	std::size_t readFramesI16(std::size_t pos, std::size_t count, std::int16_t *out) {
		std::size_t read = 0;
		std::int16_t *cursor = out;

		if (pos > this->buffer.getLength()) return 0;
		/* can't read more than what's remaining in the buffer. */
		count = std::min(this->buffer.getLength() - pos, count);
		while (count - read > 0) {
			std::size_t actual_pos = pos + read;
			/* The overhead of always getting the chunk is minimal, so let's avoid bug-prone branches and always do it. */
			this->chunk = this->buffer.getChunk(actual_pos);
			std::size_t chunk_off = actual_pos - this->chunk.start;
			std::size_t chunk_avail = this->chunk.end - chunk_off;
			std::size_t will_copy = std::min(chunk_avail, count - read);
			std::int16_t *ptr = this->chunk.data + chunk_off * this->channels;
			std::copy(ptr, ptr + will_copy * channels, cursor);
			cursor += will_copy;
			read += will_copy;
		}

		return read;
	}

	/*
	 * Read frames into an AudioSample. If the workspace and workspace_len parameters are specified, they will be used as scratch space for the intermediate conversion.
	 * They exist to allow for more than the default stack-allocated workspace size, which can speed up the loops here significantly.
	 * workspace must be aligned and workspace_len is in elements, not frames.
	 * */
	std::size_t readFrames(std::size_t pos, std::size_t count, AudioSample *out, std::size_t workspace_len = 0, std::int16_t *workspace = nullptr) {
		alignas(config::ALIGNMENT) std::array<std::int16_t, config::MAX_CHANNELS * 16> default_workspace = { 0 };

		if (workspace_len * this->channels < default_workspace.size() || workspace == nullptr) {
			workspace = &default_workspace[0];
			workspace_len = default_workspace.size();
		}

		std::size_t workspace_frames = workspace_len / this->channels;
		std::size_t written = 0;
		AudioSample *cursor = out;

		while (written < count) {
			std::size_t requested = std::min(count - written, workspace_frames);
			std::size_t got = this->readFramesI16(pos + written, requested, workspace);
			for (unsigned int i = 0; i < got * this->channels; i++) {
				cursor[i] = workspace[i] / 32768.0f;
			}
			cursor += got * this->channels;
			if (got < requested) {
				break;
			}
		}

		return written;
	}

	private:
	BufferRef buffer;
	/* We hold a copy to avoid going through two pointers and because it's unlikely that compilers can tell this never changes. */
	unsigned int channels;
	BufferChunkRef chunk;
};

class AudioDecoder;
std::shared_ptr<Buffer> bufferFromDecoder(const std::shared_ptr<AudioDecoder> &decoder);

}
