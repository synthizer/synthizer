/*
 * This file defines macros to apply compiler attributes.
 * 
 * The purpose is to allow Synthizer to one day be cross-compiler if necessary, though for now these are just Clang.
 * */

/* Inline all of a function's callees if possible. */
#define FLATTENED [[gnu::flatten]]
