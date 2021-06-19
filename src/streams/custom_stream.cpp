#include "synthizer.h"

#include "synthizer/byte_stream.hpp"
#include "synthizer/c_api.hpp"
#include "synthizer/error.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/memory.hpp"

#include <memory>
#include <sstream>
#include <string>

namespace synthizer {

/**
 * A byte stream from a set of user callbacks.
 * */
class CustomByteStream: public ByteStream {
	public:
	CustomByteStream(struct syz_CustomStreamDef callbacks): callbacks(callbacks) {}
	~CustomByteStream();

	std::string getName() override;
	unsigned long long read(unsigned long long count, char *destination) override;
	bool supportsSeek() override;
	unsigned long long getPosition() override;
	void seek(unsigned long long pos) override;
	unsigned long long  getLength() override;

	private:
	struct syz_CustomStreamDef callbacks;
	unsigned long long position = 0;
};

std::string formatCustomByteStreamError(int code, const char *err_msg) {
	std::ostringstream msg{};
	msg << "Custom byte stream error" << code;
	if (err_msg) {
		msg << err_msg;
	}
	return msg.str();
}

void throwCustomByteStreamError(int code, const char *err_msg) {
	auto msg = formatCustomByteStreamError(code, err_msg);
	throw EByteStreamCustom(msg);
}

CustomByteStream::~CustomByteStream() {
	const char *err_msg;
	int code;

	if (this->callbacks.close_cb == nullptr) {
		return;
	}

	code = this->callbacks.close_cb(this->callbacks.userdata, &err_msg);
	if (code != 0) {
		auto msg = formatCustomByteStreamError(code, err_msg);
		logError("Error closing custom byte stream: %s", msg.c_str());
	}

	if (this->callbacks.destroy_cb != nullptr) {
		this->callbacks.destroy_cb(this->callbacks.userdata);
	}
}

std::string CustomByteStream::getName() {
	return "custom";
}

unsigned long long CustomByteStream::read(unsigned long long count, char *destination) {
	unsigned long long got = 0;
	int code;
	const char *err_msg = NULL;
	code = this->callbacks.read_cb(&got, count, destination, this->callbacks.userdata, &err_msg) != 0;
	if (code != 0) {
		throwCustomByteStreamError(code, err_msg);
	}
	this->position += got;
	return got;
}

bool CustomByteStream::supportsSeek() {
	return this->callbacks.seek_cb != NULL && this->callbacks.length >= 0;
}

unsigned long long CustomByteStream::getPosition() {
	return this->position;
}

unsigned long long CustomByteStream::getLength() {
	return this->callbacks.length >= 0 ? this->callbacks.length : 0;
}

void CustomByteStream::seek(unsigned long long pos) {
	if (this->callbacks.length < 0 || this->callbacks.seek_cb == NULL) {
		throw EByteStreamUnsupportedOperation();
	}

	int code = 0;
	const char *err_msg;
	code = this->callbacks.seek_cb(pos, this->callbacks.userdata, &err_msg);
	if (code != 0) {
		throwCustomByteStreamError(code, err_msg);
	}
	this->position = pos;
}

std::shared_ptr<ByteStream> customStream(struct syz_CustomStreamDef *def) {
	if (def->read_cb == nullptr) {
		throw EValidation("Missing read callback");
	}

	/* All other callbacks are optional. */
	auto s = sharedPtrDeferred<CustomByteStream>(new CustomByteStream(*def), deferredDelete<CustomByteStream>);
	return std::static_pointer_cast<ByteStream>(s);
}

}

using namespace synthizer;

SYZ_CAPI syz_ErrorCode syz_registerStreamProtocol(const char *protocol, syz_StreamOpenCallback *callback, void *userdata) {
	SYZ_PROLOGUE
	/**
	 * Copy this so that the user doesn't need to keep the string alive.
	 * */
	std::string pname = protocol;
	auto factory = [=](const char *path, void *param) -> std::shared_ptr<ByteStream> {
		syz_CustomStreamDef def{};
		int code;
		const char *err_msg = nullptr;
		code = callback(&def, pname.c_str(), path, param, userdata, &err_msg);
		if (code != 0) {
			throwCustomByteStreamError(code, err_msg);
		}
		return customStream(&def);
	};
	registerByteStreamProtocol(pname, factory);
	return 0;
	SYZ_EPILOGUE
}
