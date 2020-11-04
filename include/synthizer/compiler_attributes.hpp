/*
 * This file defines macros to apply compiler attributes.
 * 
 * The purpose is to allow Synthizer to one day be cross-compiler if necessary, though for now these are just Clang.
 * */

/* Inline all of a function's callees if possible. */
#define FLATTENED [[gnu::flatten]]

/*
 * This function is printf-like. Argument m (1-based) is the format string, argument n is the first argument to be used for formatting
 * 
 * Note: don't use this on class metods.
 * */
#define PRINTF_LIKE(m, n) [[gnu::format(printf, m, n)]]
