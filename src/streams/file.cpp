#include "synthizer/byte_stream.hpp"

#include <fstream>
#include <utility>
#include <vector>
#include <tuple>
#include <memory>

/*
 * Filesystem is flaky on older versions of GCC, etc. In those cases CMake will tell Synthizer
 * to use the pre-C++17 fallback of a string path, which might break unicode on Windows
 * Since we require very recent MSVC on Windows, however, that's fine: unicode will still work there.
 * */
#ifdef SYZ_USE_FILESYSTEM
#include <filesystem>
#endif

namespace synthizer {

class FileByteStream: public ByteStream {
	public:
	FileByteStream(std::fstream &&f);

	std::string getName() override;
	unsigned long long read(unsigned long long count, char *destination) override;
	bool supportsSeek() override;
	unsigned long long getPosition() override;
	void seek(unsigned long long position) override;
	unsigned long long getLength() override;

	private:
	std::fstream stream;
	unsigned long long length = 0;
};

FileByteStream::FileByteStream(std::fstream &&s): stream(std::move(s)) {
	this->stream.seekg(0, std::ios_base::end);
	this->length = this->stream.tellg();
	this->stream.seekg(0, std::ios_base::beg);
}

std::string FileByteStream::getName() {
	return "file";
}

unsigned long long FileByteStream::read(unsigned long long count, char *destination) {
	unsigned long long read = 0;
	int retries = 5;
	if (count == 0)
		return 0;
	do {
		this->stream.read(destination+read, count-read);
		read += this->stream.gcount();
		if (read == count || this->stream.eof())
			break;
		retries --;
	} while(retries > 0);
	return read;
}

bool FileByteStream::supportsSeek() {
	return true;
}

unsigned long long FileByteStream::getPosition() {
	// Have to clear eofbit or we can't get the position, even though eof has a defined position.
	this->stream.clear();
	auto pos = this->stream.tellg();
	if (pos < 0)
		throw EByteStream("Unable to get file position. This should probably never happen.");
	return pos;
}

void FileByteStream::seek(unsigned long long position) {
	this->stream.clear();
	this->stream.seekg(position);
	if (this->stream.good() != true)
		throw EByteStream("Unable to seek.");
}

unsigned long long FileByteStream::getLength() {
	return this->length;
}

std::shared_ptr<ByteStream> fileStream(const char *path, void *param) {
	(void)param;

	std::fstream stream;

#ifdef SYZ_USE_FILESYSTEM
	auto p = std::filesystem::u8path(path);
	stream.open(p, std::ios::in|std::ios::binary);

	if (std::filesystem::exists(p) == false)
		throw EByteStreamNotFound();
#else
	stream.open(path, std::ios::in|std::ios::binary);
	/*
	 * This path doesn't have a good way to know if the open failed due to the file not
	 * existing; we'll just have to throw the generic error below.
	 * */
#endif
	if (stream.is_open() == false)
		throw EByteStream("Unable to open file");
	return std::make_shared<FileByteStream>(std::move(stream));
}

}
