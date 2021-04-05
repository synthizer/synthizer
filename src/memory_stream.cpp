#include "synthizer/byte_stream.hpp"

#include "synthizer/memory.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace synthizer {

class MemoryStream: public ByteStream {
	public:
	MemoryStream(std::size_t length, const char *data): data(data), length(length), position(0) {}

	std::string getName() override;
	std::size_t read(std::size_t count, char *destination) override;
	bool supportsSeek() override;
	std::size_t getPosition() override;
	void seek(std::size_t position) override;
	std::size_t getLength() override;

	private:
	const char *data;
	std::size_t length, position;
};

std::string MemoryStream::getName() {
	return "memory";
}

std::size_t MemoryStream::read(std::size_t count, char *destination) {
	std::size_t remaining = this->length - this->position;
	/* Don't read past the end. */
	std::size_t will_read = remaining > count ? count : remaining;
	std::copy(this->data + this->position, this->data + this->position + will_read, destination);
	this->position += will_read;
	return will_read;
}

bool MemoryStream::supportsSeek() {
	return true;
}

std::size_t MemoryStream::getPosition() {
	return this->position;
}

void MemoryStream::seek(std::size_t pos) {
	if (pos > this->length) {
		throw EByteStream("Out of range seek");
	}
	this->position = pos;
}

std::size_t MemoryStream::getLength() {
	return this->length;
}

std::shared_ptr<ByteStream> memoryStream(std::size_t size, const char *data) {
	auto stream = sharedPtrDeferred<MemoryStream>(new MemoryStream(size, data), [](auto *p) { deferredDelete(p); });
	return stream;
}

}
