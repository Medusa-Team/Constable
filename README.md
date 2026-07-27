Constable
=========
This is partially ported version of Constable 64-bit from 32-bit version of Constable.
It is still under developement.

Constable is the authorization server for security system Medusa Voyager that runs in user space.
It is a process that decides which actions will Medusa permit or not. It is the only process, 
that is excluded from Medusa. Constable configuration consists of two parts:

0. Constable configuration
0. Configuration of rules for security system Medusa

Constable is completely independent from kernel, which is ensured by kernel sending all supported 
entities to Constable at the start.

Long-running decisions
----------------------

Linux Medusa's decision timeout is a renewable liveness lease rather than a
maximum decision duration. An asynchronous or interactive handler may call
`mcp_renew_authrequest(request)` before each lease expires while it waits for
user input. The progress message extends only that request's lease; it does
not send a verdict or change policy.

This is currently an optional protocol-v3 extension. Callers must enable it
only when paired with a kernel that supports
`MEDUSA_COMM_AUTHREQUEST_PROGRESS`; automatic feature negotiation is planned
for the next protocol revision.

Protocol-v3 input validation
----------------------------

Constable treats dynamically announced class, event, operand, and attribute
names as fixed-width wire fields, not trusted C strings. Each field must
contain a terminating NUL; class, event, and attribute names must be non-empty;
and every non-terminal attribute must have a positive width contained within
its announced object. Malformed definitions close the connection rather than
being silently truncated into a different policy name.

Policy handler names are also checked against the protocol-v3 operation-name
width. An overlong function, event, or tree handler is rejected during policy
compilation with a source diagnostic.

Development checks
------------------

Constable and libmcompiler default to strict ISO C11 with
`-Wall -Wextra -Wpedantic -pedantic-errors`. Production builds do not enable
`DEBUG_TRACE`; developers who need the historical trace output can add
`-DDEBUG_TRACE` explicitly to their `CFLAGS`. CI builds the complete tree with
both GCC and Clang and promotes every warning to an error.

The normal Linux build and semantic corpus run with:

```sh
make -C libmcompiler
make -C constable
make -C constable test
```

CI also builds the complete reference server and executes the same tests
independently under AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
./ci/sanitize.sh address
./ci/sanitize.sh undefined
```

The Linux-2.x force-code add-on and incomplete C preprocessor have been
retired. Their rationale and the boundary enforced by tests are documented in
[docs/retired-components.md](docs/retired-components.md). The retained
mini-libc formatter is bounded and unit-tested independently; it is not a
supported force-code execution path.

Policy language
---------------

The protocol-v3 policy grammar, handler ordering, and multi-handler answer
composition are documented in [docs/policy-language.md](docs/policy-language.md).
The document records implemented behavior so later compiler modernization can
be checked for semantic equivalence.

Preserved policies and their compiled virtual-space semantics are covered by
the compatibility corpus described in
[docs/policy-compatibility.md](docs/policy-compatibility.md).

Usage
-----
run constable with parameter minimal/constable.conf that blocks all syscalls
```
constable minimal/constable.conf
```
At this time, the only supported syscall in Medusa Voyager is symlink
