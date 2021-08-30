/*
 * Declares prototypes for property methods.
 *
 * This is part of the nwe property infrastructure, which is being built in several pieces. In future,
 * this file will work like property_impl.hpp, where you define some parameter macros and include it.
 *
 * For the moment, just include it in your class in the public methods, like the old PROPERTY_METHODS macro.
 * */

bool hasProperty(int property) override;
property_impl::PropertyValue getProperty(int property) override;
void validateProperty(int property, const property_impl::PropertyValue &value) override;
void setProperty(int property, const property_impl::PropertyValue &value) override;
