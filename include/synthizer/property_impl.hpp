/*
 * Defines property methods on a class.
 * 
 * This header can be included more than once, and should be included outside the synthizer namespace.
 * */

#include "synthizer_constants.h"
#include "synthizer_properties.h"

#include "synthizer/base_object.hpp"
#include "synthizer/error.hpp"
#include "synthizer/property_internals.hpp"

#include <memory>
#include <variant>

namespace synthizer {

#ifndef PROPERTY_CLASS
#error "When implementing properties, must define PROPERTY_CLASS to the class the properties are being added to."
#endif

#ifndef PROPERTY_BASE
#error "Forgot to define PROPERTY_BASE when implementing properties. Define this to your immediate base class."
#endif

#ifndef PROPERTY_LIST
#error "Need PROPERTY_LIST defined to know where to get properties from"
#endif

#define P_INT_MIN property_impl::int_min
#define P_INT_MAX property_impl::int_max
#define P_DOUBLE_MIN property_impl::double_min
#define P_DOUBLE_MAX property_impl::double_max

/*
 * The implementation itself is a giant switch, with a call in the default case to the base for 3 methods.
 * 
 * These are the stanzas for those methods.
 * */
#define HAS_(P, ...) \
case (P): return true; \

#define GET_CONV_(t, conv, p, name1, name2, ...) \
case (p): { \
	auto tmp = this->get##name2(); \
	property_impl::PropertyValue ret = conv(tmp); \
	auto ptr = std::get_if<t>(&ret); \
	if (ptr == nullptr) throw EPropertyType(); \
	return ret; \
} \
break;

#define GET_(t, ...) GET_CONV_(t, [](auto x) { return x; }, __VA_ARGS__)

#define SET_(type, checker, p, name1, name2, ...) \
case (p): { \
	auto ptr = std::get_if<type>(&value); \
	if (ptr == nullptr) throw EPropertyType(); \
	this->set##name2(checker(ptr)); \
} \
break;

/* Now implement the methods. */

#define INT_P(...) HAS_(__VA_ARGS__)
#define DOUBLE_P(...) HAS_(__VA_ARGS__)
#define OBJECT_P(...) HAS_(__VA_ARGS__)

bool PROPERTY_CLASS::hasProperty(int property) {
	switch (property) {
	PROPERTY_LIST
	default: 
		PROPERTY_BASE::hasProperty(property);
	}

	return false;
}

#undef INT_P
#undef DOUBLE_P
#undef OBJECT_P
#define INT_P(...) GET_(int, __VA_ARGS__)
#define DOUBLE_P(...) GET_(double, __VA_ARGS__)

/* Getting objects is different; we have to cast to the base class. */
#define OBJECT_P(p, ...) GET_CONV_(std::shared_ptr<BaseObject>, [] (auto &x) { return std::static_pointer_cast<BaseObject>(x); }, __VA_ARGS__)

property_impl::PropertyValue PROPERTY_CLASS::getProperty(int property) {
	switch (property) {
	PROPERTY_LIST
	default:
		return PROPERTY_BASE::getProperty(property);
	}
}

#undef INT_P
#undef DOUBLE_P
#undef OBJECT_P

#define INT_P(p, name1, name2, min, max) SET_(int, [](auto x) { if(*x < min || *x >= max) throw ERange(); return *x; }, p, name1, name2, min, max);
#define DOUBLE_P(p, name1, name2, min, max) SET_(double, [](auto x) { if (*x < min || *x >= max) throw ERange(); return *x; }, p, name1, name2, min, max)
#define OBJECT_P(p, name1, name2, cls) SET_(std::shared_ptr<BaseObject>, [] (auto *x) { auto &y = *x; auto z = std::dynamic_pointer_cast<cls>(y); if (z == nullptr) throw EType(); return z; }, p, name1, name2, cls)

void PROPERTY_CLASS::setProperty(int property, const property_impl::PropertyValue &value) {
	switch (property) {
	PROPERTY_LIST
	default:
		PROPERTY_BASE::setProperty(property, value);
	}
}

#undef PROPERTY_CLASS
#undef PROPERTY_BASE
#undef PROPERTY_LIST
#undef INT_P
#undef DOUBLE_P
#undef OBJECT_P
#undef HAS_
#undef GET_
#undef _GET_CONV_
#undef SET_
#undef INT_MIN
#undef INT_MAX
#undef FLOAT_MIN
#undef FLOAT_MAX

}
