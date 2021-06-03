#include "synthizer/base_object.hpp"
#include "synthizer/context.hpp"

#include <memory>

namespace synthizer {


void BaseObject::stopLingering() {
	this->context->enqueueLingerStop(std::static_pointer_cast<BaseObject>(this->linger_reference));
}

}
