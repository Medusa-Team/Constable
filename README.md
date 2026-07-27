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

Development checks
------------------

Constable and libmcompiler default to strict ISO C11 with
`-Wall -Wextra -Wpedantic -pedantic-errors`. Production builds do not enable
`DEBUG_TRACE`; developers who need the historical trace output can add
`-DDEBUG_TRACE` explicitly to their `CFLAGS`.

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

Policy language
---------------

The protocol-v3 policy grammar, handler ordering, and multi-handler answer
composition are documented in [docs/policy-language.md](docs/policy-language.md).
The document records implemented behavior so later compiler modernization can
be checked for semantic equivalence.

Usage
-----
run constable with parameter minimal/constable.conf that blocks all syscalls
```
constable minimal/constable.conf
```
At this time, the only supported syscall in Medusa Voyager is symlink
