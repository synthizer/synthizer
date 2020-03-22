#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include <tuple>
#include <memory>

#include "synthizer/byte_stream.hpp"

namespace synthizer {

class FileByteStream: public ByteStream {
	public:
	FileByteStream(std::fstream &&f);

	std::string getName();
	std::size_t read(std::size_t count, char *destination);
	bool supportsSeek();
	std::size_t getPosition();
	void seek(std::size_t position);

	private:
	std::fstream stream;
};

FileByteStream::FileByteStream(std::fstream &&s): stream(std::move(s)) {
}

std::string FileByteStream::getName() {
	return "file";
}

std::size_t FileByteStream::read(std::size_t count, char *destination) {
	std::size_t read = 0;
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

std::size_t FileByteStream::getPosition() {
	auto pos = this->stream.tellg();
	if (pos < 0)
		throw EByteStream("Unable to get file position. This should probably never happen.");
	return pos;
}

void FileByteStream::seek(std::size_t position) {
	this->stream.clear();
	this->stream.seekg(position);
	if (this->stream.good() != true)
		throw EByteStream("Unable to seek.");
}

std::shared_ptr<ByteStream> fileStream(const std::string &path, const std::vector<std::tuple<std::string, std::string>> &options) {
	(void) options;

	auto p = std::filesystem::u8path(path);

	if (std::filesystem::exists(p) == false)
		throw EByteStreamNotFound();

	std::fstream stream;
	stream.open(p, std::ios::in|std::ios::binary);
	if (stream.is_open() == false)
		throw EByteStream("File path existed, but we were unable to open.");
	return std::make_shared<FileByteStream>(std::move(stream));
}

}
