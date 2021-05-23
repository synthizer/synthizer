#include "synthizer/byte_stream.hpp"
#include "synthizer/channel_mixing.hpp"
#include "synthizer/config.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/error.hpp"
#include"synthizer/logging.hpp"
#include "synthizer/memory.hpp"
#include "synthizer/shared_object.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>

#include <stdio.h>
#include <stdint.h>

/**
 * Libsndfile functions and prototypes.
 * */

/**
 * It's not obvious from the libsndfile autoconf and headers, but sf_count_t is actually always int64_t, save on systems so ancient that no one uses them anymore.
 * */
typedef int64_t sf_count_t;

typedef	struct SNDFILE_tag	SNDFILE ;
typedef sf_count_t		(*sf_vio_get_filelen)	(void *user_data) ;
typedef sf_count_t		(*sf_vio_seek)		(sf_count_t offset, int whence, void *user_data) ;
typedef sf_count_t		(*sf_vio_read)		(void *ptr, sf_count_t count, void *user_data) ;
typedef sf_count_t		(*sf_vio_write)		(const void *ptr, sf_count_t count, void *user_data) ;
typedef sf_count_t		(*sf_vio_tell)		(void *user_data) ;

typedef struct {
	sf_vio_get_filelen  get_filelen ;
	sf_vio_seek         seek ;
	sf_vio_read         read ;
	sf_vio_write        write ;
	sf_vio_tell         tell ;
} SF_VIRTUAL_IO;

typedef struct {    sf_count_t  frames ;     /* Used to be called samples. */
	int         samplerate ;
	int         channels ;
	int         format ;
	int         sections ;
	int         seekable ;
} SF_INFO;
enum {
	SFM_READ	= 0x10,
	SFM_WRITE	= 0x20,
	SFM_RDWR	= 0x30,

	SF_SEEK_SET = SEEK_SET,
	SF_SEEK_CUR = SEEK_CUR,
	SF_SEEK_END = SEEK_END
};


typedef SNDFILE* 	sf_open_virtual_fn(SF_VIRTUAL_IO *sfvirtual, int mode, SF_INFO *sfinfo, void *user_data) ;
typedef int  sf_close_fn(SNDFILE *sndfile) ;
typedef sf_count_t  sf_seek_fn(SNDFILE *sndfile, sf_count_t frames, int whence) ;
typedef sf_count_t  sf_readf_float_fn(SNDFILE *sndfile, float *ptr, sf_count_t items) ;
typedef int sf_error_fn(SNDFILE *sndfile);
typedef const char* sf_strerror_fn(SNDFILE *sndfile) ;

namespace synthizer {

/**
 * Pointers to the libsndfile functions. Initialized as part of library initialization.
 * */
static sf_open_virtual_fn *sf_open_virtual = nullptr;
static sf_close_fn *sf_close = nullptr;
static sf_seek_fn *sf_seek = nullptr;
static sf_readf_float_fn *sf_readf_float = nullptr;
static sf_error_fn *sf_error = nullptr;
static sf_strerror_fn *sf_strerror = nullptr;

static bool libsndfile_loaded = false;
static SharedObject *libsndfile_obj = nullptr;

static sf_count_t snd_filelen_cb(void *userdata) {
	ByteStream *stream = (ByteStream *)userdata;
	return stream->getLength();
}

static sf_count_t snd_seek_cb(sf_count_t offset, int whence, void *userdata) {
	ByteStream *stream = (ByteStream *)userdata;

	auto cur_pos = stream->getPosition();
	unsigned long long new_pos = cur_pos;

	switch (whence) {
	case SF_SEEK_CUR:
		new_pos = (unsigned long long)((long long)cur_pos + offset);
		break;
	case SF_SEEK_SET:
		new_pos = (unsigned long long)offset;
		break;
	case SF_SEEK_END:
		new_pos = static_cast<long long>(stream->getLength()) + offset;
		break;
	default:
		assert(0 && "Libsndfile gave us an invalid seek mode");
	}

	try {
		stream->seek(new_pos);
	} catch(...) {
		logError("Error seeking in Libsndfile decoder");
		/*
		 * It may not always be the case that the pointer didn't move, but it's the best we can do.
		 */
		return cur_pos;
	}

	return new_pos;
}

static sf_count_t snd_read_cb(void *ptr, sf_count_t count, void *userdata) {
	char *dest = (char *)ptr;
	ByteStream *stream = (ByteStream *)userdata;

	try {
		return stream->read(count, dest);
	} catch(...) {
		logError("Error reading stream in libsndfile callbacks");
		return 0;
	}
}

sf_count_t snd_tell_cb(void *userdata) {
	return ((ByteStream *)userdata)->getLength();
}

/**
 * Libsndfile error handling is interesting because sf_strerror doesn't return NULL for no error. This function
 * does the error check and throws an exception if file has an error state, and should be called
 * after most libsndfile operations.
 * */
static void throwForLibsndfile(SNDFILE *file, const char *why) {
	if (sf_error(file) != 0) {
		const char *e = sf_strerror(file);
		throw Error(std::string(why) + std::string(" ") + std::string(e));
	}
}

class LibsndfileDecoder: public AudioDecoder {
	public:
	LibsndfileDecoder(const std::shared_ptr<LookaheadByteStream> &stream);
	~LibsndfileDecoder();

	unsigned long long writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels) override;
	void seekPcm(unsigned long long frame) override;
	bool supportsSeek() override;
	bool supportsSampleAccurateSeek() override;
	unsigned long long getLength() override;
	int getSr() override;
	int getChannels() override;
	AudioFormat getFormat() override;

	private:
	SNDFILE *file;
	std::shared_ptr<ByteStream> stream;
	SF_INFO info{};
	std::array<float, config::BLOCK_SIZE * config::MAX_CHANNELS> tmp_buf {{0}};
};

LibsndfileDecoder::LibsndfileDecoder(const std::shared_ptr<LookaheadByteStream> &s) {
	this->stream = std::static_pointer_cast<ByteStream>(s);
	SF_VIRTUAL_IO callbacks = SF_VIRTUAL_IO {
		snd_filelen_cb,
		snd_seek_cb,
		snd_read_cb,
		nullptr,
		snd_tell_cb,
	};

	this->file = sf_open_virtual(&callbacks, SFM_READ, &this->info, (void *)this->stream.get());
	if (this->file == nullptr) {
		throw Error(std::string("Unable to open Libsndfile virtual file: ") + std::string(sf_strerror(nullptr)));
	}
	if (this->info.channels > config::MAX_CHANNELS) {
		if (sf_close(this->file) != 0) {
			// Shouldn't be possible.
			logError("Unable to close Libsndfile file: %s", sf_strerror(this->file));
		}
		throw Error("Too many channels");
	}

	s->resetFinal();
}

LibsndfileDecoder::~LibsndfileDecoder() {
	if (sf_close(this->file) != 0) {
		// Shouldn't be possible.
		logError("Unable to close Libsndfile file: %s", sf_strerror(this->file));
	}
}

unsigned long long LibsndfileDecoder::writeSamplesInterleaved(unsigned long long num, float *samples, unsigned int channels) {
	auto actualChannels = channels < 1 ? this->info.channels : channels;

	/* Fast case: if the channels are equal, just write. */
	if (actualChannels == (unsigned int)this->info.channels) {
		unsigned long long ret = sf_readf_float(this->file, samples, num);
		throwForLibsndfile(this->file, "Unable to read");
		return ret;
	}

	/* Otherwise we have to round trip via the temporary buffer. */
	std::fill(samples, samples + num * actualChannels, 0.0f);

	unsigned long long needed = num;
	const unsigned long long tmp_buf_frames = this->tmp_buf.size() / this->info.channels;
	while (needed > 0) {
		sf_count_t will_read = std::min(needed, tmp_buf_frames);
		unsigned long long got = sf_readf_float(this->file, &this->tmp_buf[0], will_read);
		throwForLibsndfile(this->file, "Unable to read");
		if (got == 0) {
			break;
		}
		mixChannels(got, &this->tmp_buf[0], this->info.channels, samples + (num - needed) * actualChannels, actualChannels);
	}
	return num - needed;
}

void LibsndfileDecoder::seekPcm(unsigned long long pos) {
	sf_seek(this->file, pos, SF_SEEK_SET);
	throwForLibsndfile(this->file, "Unable to seek");
}

bool LibsndfileDecoder::supportsSeek() {
	return true;
}

bool LibsndfileDecoder::supportsSampleAccurateSeek() {
	// Libsndfile doesn't tell us, but claiming no is always safe.
	return false;
}

unsigned long long LibsndfileDecoder::getLength() {
	return (unsigned long long)this->info.frames;
}

int LibsndfileDecoder::getSr() {
	return (int)this->info.samplerate;
}

int LibsndfileDecoder::getChannels() {
	return this->info.channels;
}

AudioFormat LibsndfileDecoder::getFormat() {
	// Libsndfile does a lot of these, let's not even try for now.
	return AudioFormat::Unknown;
}

void loadLibsndfile(const char *path) {
	libsndfile_obj = new SharedObject(path);
	sf_open_virtual = (sf_open_virtual_fn *)libsndfile_obj->getSymbol("sf_open_virtual");
	sf_close = (sf_close_fn *)libsndfile_obj->getSymbol("sf_close");
	sf_seek = (sf_seek_fn *)libsndfile_obj->getSymbol("sf_seek");
	sf_readf_float = (sf_readf_float_fn *)libsndfile_obj->getSymbol("sf_readf_float");
	sf_error = (sf_error_fn *)libsndfile_obj->getSymbol("sf_error");
	sf_strerror = (sf_strerror_fn *)libsndfile_obj->getSymbol("sf_strerror");
	libsndfile_loaded = true;
}

std::shared_ptr<AudioDecoder> decodeLibsndfile(std::shared_ptr<LookaheadByteStream> stream) {
	if (libsndfile_loaded == false) {
		logDebug("decoder: skipping libsndfile because it isn't loaded");
		return nullptr;
	}
	if (stream->supportsSeek() == false) {
		logDebug("Libsndfile: skipping because the stream must support seeking");
	}

	std::shared_ptr<AudioDecoder> res = sharedPtrDeferred<AudioDecoder>(new LibsndfileDecoder(stream), deferredDelete<AudioDecoder>);
	return res;
}

}
