# Introduction

The following tutorials demonstrate installation and basic Synthizer usage
through the Python bindings, in order to let you get a basic feel for the
library.  This manual primarily documents from the perspective of the C API,
since it is infeasible to write a version of this manual for every binding that
might exist.  In general the C API maps to the Python bindings in a
straightforward fashion, though resources prevent maintaining a fully documented
Python API reference.

Nonetheless these tutorials privilege Python.  The Python bindings are special
for two reasons: first, they're the official way I test Synthizer, and thus
always up to date.  Second, they're a good way to show how the various Synthizer
pieces fit together without having to also demonstrate C-style error checking
and similar boilerplate that gets in the way of learning.
