#pragma once

/*
 * This file defines macros to apply compiler attributes.
 *
 * The purpose is to allow Synthizer to one day be cross-compiler if necessary, though for now these are just Clang.
 * */

/* Inline all of a function's callees if possible. */
#define FLATTENED [[gnu::flatten]]

/*
 * This function is printf-like. Argument m (1-based) is the format string, argument n is the first argument to be used
 * for formatting
 *
 * Note: don't use this on class methods.
 * */
#define PRINTF_LIKE(m, n) [[gnu::format(printf, m, n)]]

/* For MSVC, introduce defines to the Clang builtins. */
#if defined(_MSC_VER) && !(defined(__clang__) || defined(__GNUC__))
#include <intrin.h>

#define __builtin_popcount(x) __popcnt(x)

static inline int __builtin_ctz(unsigned x) {
  unsigned long ret;
  _BitScanForward(&ret, x);
  return (int)ret;
}
#endif
