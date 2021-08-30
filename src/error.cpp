#include "synthizer/error.hpp"

namespace synthizer {

Error::Error(const std::string &message) : message(message) {}

const std::string &Error::getMessage() const { return this->message; }

} // namespace synthizer
