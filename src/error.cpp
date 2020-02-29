#include "synthizer/error.hpp"

namespace synthizer {

Error::Error(std::string message): message(message) {
}

const std::string &Error::getMessage() const {
	return this->message;
}

}

