#include <memory>
#include <cstdlib>
#include <algorithm>

#include "synthizer/byte_stream.hpp"


namespace synthizer {

class MemoryByteStream: public ByteStream {
	public:
	MemoryByteStream(std::size_t size, char *data);
	std::string getName();
	std::size_t read(std::size_t count, char *destination);
	bool supportsSeek();
	std::size_t getPosition();
	void seek(std::size_t position);

	private:
	std::size_t length, position;
	char *data;
};

MemoryByteStream::MemoryByteStream(std::size_t length, char *data): length(length), position(0), data(data) {
}

std::string MemoryByteStream::getName() {
	return "memory";
}

std::size_t MemoryByteStream::read(std::size_t count, char *destination) {
	std::size_t will_read = std::min(count, this->length-this->position);
	std::copy(this->data+position, this->data+position+count, destination);
	this->position += count;
	return count;
}

bool MemoryByteStream::supportsSeek() {
	return true;
}

std::size_t MemoryByteStream::getPosition() {
	return this->position;
}

void MemoryByteStream::seek(std::size_t position) {
	if (position >= this->length)
		throw EByteStreamUnsupportedOperation("Attempt to seek past end of stream");
	this->position = position;
}

}
