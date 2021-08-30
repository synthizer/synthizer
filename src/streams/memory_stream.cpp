#include "synthizer/byte_stream.hpp"

#include "synthizer/memory.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

namespace synthizer {

class MemoryStream : public ByteStream {
public:
  MemoryStream(std::size_t length, const char *data) : data(data), length(length), position(0) {}

  std::string getName() override;
  unsigned long long read(unsigned long long count, char *destination) override;
  bool supportsSeek() override;
  unsigned long long getPosition() override;
  void seek(unsigned long long position) override;
  unsigned long long getLength() override;

private:
  const char *data;
  std::size_t length, position;
};

std::string MemoryStream::getName() { return "memory"; }

unsigned long long MemoryStream::read(unsigned long long count, char *destination) {
  std::size_t remaining = this->length - this->position;
  /* Don't read past the end. */
  std::size_t will_read = remaining > count ? count : remaining;
  std::copy(this->data + this->position, this->data + this->position + will_read, destination);
  this->position += will_read;
  return will_read;
}

bool MemoryStream::supportsSeek() { return true; }

unsigned long long MemoryStream::getPosition() { return this->position; }

void MemoryStream::seek(unsigned long long pos) {
  if (pos > this->length) {
    throw EByteStream("Out of range seek");
  }
  this->position = pos;
}

unsigned long long MemoryStream::getLength() { return this->length; }

std::shared_ptr<ByteStream> memoryStream(std::size_t size, const char *data) {
  auto stream = sharedPtrDeferred<MemoryStream>(new MemoryStream(size, data), [](auto *p) { deferredDelete(p); });
  return stream;
}

} // namespace synthizer
