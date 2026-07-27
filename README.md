# Constable

Constable is the readable C reference authorization server for Medusa. The
kernel announces its object classes and events at connection time; Constable
compiles a policy, maintains the userspace model of virtual spaces, and answers
the decisions that cannot be resolved by the kernel's installed baseline or
cache.

The repository preserves protocol v3 and its policy language while providing a
strict ISO C11 build, sanitizer coverage, offline policy inspection, active
kernel-surface validation, and semantic tests for historical policies. See
[the architecture guide](docs/architecture.md) before changing ownership,
threading, request, or policy-evaluation code.

## Build and run

```sh
make -C libmcompiler
make -C constable
make -C constable test
```

For an offline policy build:

```sh
constable/constable -t -c constable/minimal/medusa.conf \
  constable/minimal/constable.conf
```

Run `constable/constable --help` for all command-line modes. Production use
requires a matching Medusa kernel and normally supplies both the Constable
communication configuration and the Medusa policy configuration.

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

## Development checks

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

## Policy language

The protocol-v3 policy grammar, handler ordering, and multi-handler answer
composition are documented in [docs/policy-language.md](docs/policy-language.md).
The document records implemented behavior so later compiler modernization can
be checked for semantic equivalence.

Preserved policies and their compiled virtual-space semantics are covered by
the compatibility corpus described in
[docs/policy-compatibility.md](docs/policy-compatibility.md).

`constable -I` emits the compiled policy as non-mutating JSON for review and
tooling; its schema and offline guarantees are documented in
[docs/policy-inspection.md](docs/policy-inspection.md).

`constable -V /sys/kernel/security/medusa` rejects compiled policies that
reference classes or events not actively enforced by the selected kernel. Its
fail-closed behavior and inventory format are documented in
[docs/policy-validation.md](docs/policy-validation.md).

## Compatibility boundary

Protocol v3 uses dynamically announced native-layout definitions. It is kept
for the migrated Linux 7.1 baseline, not proposed as a new stable UAPI.
Protocol v4 is expected to replace it with fixed-width framed messages. The
reference implementation deliberately keeps transport, schema, policy, and
decision layers visible so that migration can be checked against the preserved
semantic corpus.
