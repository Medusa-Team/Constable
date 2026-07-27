# Retired components

Constable's supported build no longer carries two incomplete, unreachable
implementation experiments from the original Linux 2.x-era source tree.
Their history remains available in Git.

## Force-code loader

The `constable/force` loader injected executable policy code into a process.
It had already been excluded from the module list and depended on obsolete
32-bit Linux syscall, executable-format, and `vm86` assumptions. Restoring it
would both require a new implementation and conflict with the modern design
goal of keeping authorization logic in a separately confined userspace
server. Its sources were therefore retired rather than presented as a
supported security feature.

The bounded formatter in `constable/Mlibc` remains because it is independently
unit-tested. This does not make force-code loading a supported execution path.

## Incomplete C preprocessor

`libmcompiler/compiler/c_preprocesor.c` was not linked by the library build,
did not compile, and exposed an unusable `c_preprocessor_create` declaration.
Its obsolete manual test programs depended on that missing implementation.
The implementation, declaration, and manual tests were retired.

The supported compiler retains its null, file, and external-GCC preprocessor
implementations. Policy compilation is covered by the normal Constable test
suite.

## Regression boundary

`constable/tests/retired_components_test.sh` checks that neither implementation
nor its public/build entry points are accidentally restored. A future
replacement must be introduced as a reviewed, tested design rather than by
re-enabling these historical sources.
