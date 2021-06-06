#include "synthizer/base_object.hpp"
#include "synthizer/context.hpp"

#include <memory>

namespace synthizer {


void BaseObject::signalLingerStopPoint() {
	if (this->linger_reference == nullptr) {
		return;
	}

	this->context->enqueueLingerStop(std::static_pointer_cast<BaseObject>(this->linger_reference));
}

}
