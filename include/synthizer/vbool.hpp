#pragma once

#include <variant>

namespace synthizer {
/**
 * An abstraction for booleans that lets us conveniently dispatch to lambdas with std::visit based on whether a
 * condition is true.
 *
 * VBool is a variant consisting of two types, both with a constexpr operator bool so that they can be used in if
 * statements.
 * */

class VFalse {
public:
  constexpr operator bool() { return false; }
};

class VTrue {
public:
  constexpr operator bool() { return true; }
};

using VBool = std::variant<VFalse, VTrue>;

/**
 * Make a VBool, which can be used to dispatch to specialized implementations based on a condition.
 * */
inline VBool vCond(bool cond) {
  if (cond) {
    return VTrue{};
  } else {
    return VFalse{};
  }
}

} // namespace synthizer