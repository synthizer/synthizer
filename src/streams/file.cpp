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

	std::string getName() override;
	std::size_t read(std::size_t count, char *destination) override;
	bool supportsSeek() override;
	std::size_t getPosition() override;
	void seek(std::size_t position) override;
	std::size_t getLength() override;

	private:
	std::fstream stream;
	std::size_t length = 0;
};

FileByteStream::FileByteStream(std::fstream &&s): stream(std::move(s)) {
	this->stream.seekg(0, std::ios_base::end);
	this->length = this->stream.tellg();
	this->stream.seekg(0, std::ios_base::beg);
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

std::size_t FileByteStream::getLength() {
	return this->length;
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
