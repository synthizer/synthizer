#include <memory>
#include <cstdlib>
#include <algorithm>

#include "synthizer/byte_stream.hpp"


namespace synthizer {

class MemoryByteStream: public ByteStream {
	public:
	MemoryByteStream(std::size_t size, char *data);
	std::string getName();
	unsigned long long read(unsigned long long count, char *destination);
	bool supportsSeek();
	unsigned long long getPosition();
	void seek(unsigned long long position);

	private:
	std::size_t length, position;
	char *data;
};

MemoryByteStream::MemoryByteStream(std::size_t length, char *data): length(length), position(0), data(data) {
}

std::string MemoryByteStream::getName() {
	return "memory";
}

unsigned long long MemoryByteStream::read(unsigned long long count, char *destination) {
	std::size_t will_read = std::min(count, this->length-this->position);
	std::copy(this->data+position, this->data+position+will_read, destination);
	this->position += will_read;
	return will_read;
}

bool MemoryByteStream::supportsSeek() {
	return true;
}

unsigned long long MemoryByteStream::getPosition() {
	return this->position;
}

void MemoryByteStream::seek(unsigned long long pos) {
	if (pos >= this->length)
		throw EByteStreamUnsupportedOperation("Attempt to seek past end of stream");
	this->position = pos;
}

}
