#pragma once

#include <boost/predef.h>

/*
 * This file defines macros to apply compiler attributes.
 *
 * The purpose is to allow Synthizer to one day be cross-compiler if necessary, though for now these are just Clang.
 * */

/**
 * Inline all of a function's callees if possible. In Clang/Gcc, includes the inline keyword for you as well, soo this
 * should be used in the same position as and instead of inline.
 * */
#if BOOST_COMP_MSVC > 0
#define FLATTENED __forceinline
#else
#define FLATTENED [[gnu::flatten]] inline
#endif

/*
 * This function is printf-like. Argument m (1-based) is the format string, argument n is the first argument to be used
 * for formatting
 *
 * Note: don't use this on class methods.
 * */
#define PRINTF_LIKE(m, n) [[gnu::format(printf, m, n)]]

/* For MSVC, introduce defines to the Clang builtins. */
#if BOOST_COMP_MSVC > 0
#include <intrin.h>

#define __builtin_popcount(x) __popcnt(x)

static inline int __builtin_ctz(unsigned x) {
  unsigned long ret;
  _BitScanForward(&ret, x);
  return (int)ret;
}
#endif
