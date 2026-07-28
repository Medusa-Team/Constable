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

Constable sizes its decision worker pool from the number of online CPUs,
clamped to 2 through 32 workers. Use `--workers N` to select an explicit pool
size from 1 through 32 for constrained deployments or benchmarking, or
`--workers auto` to request the default explicitly. Suspended policy handlers
leave their worker and resume through the shared work queue.

Fallback policy installation is opt-in and repeatable:

```sh
constable/constable -F exec=baseline_deny \
  --fallback ptrace=online_required
```

Accepted policies are `baseline_allow`, `baseline_deny`, and
`online_required`. Constable resolves each event against the schema announced
by that kernel connection, queues every policy before READY, and refuses
startup if an event is unknown. A kernel without the optional protocol-v3
fallback command rejects the write, so a configured policy cannot silently
downgrade to the old behavior.

Non-sleepable hooks can use generation-scoped domain rules:

```sh
constable/constable \
  --domain-rule 'ptrace:7:9:*=deny' \
  --domain-rule 'sendsig:*:*:15=allow'
```

The key is `event:subject-domain:object-domain:selector`; numeric values accept
decimal or `0x` notation and `*` is a wildcard. Ptrace selectors encode
`operation << 32 | mode`, while signal selectors are signal numbers. Constable
requires protocol-v4 domain-cache negotiation whenever any such rule is
configured, so rules cannot silently degrade to synchronous delegation.

Long-running decisions
----------------------

Linux Medusa's decision timeout is a renewable liveness lease rather than a
maximum decision duration. An asynchronous or interactive handler may call
`mcp_renew_authrequest(request)` before each lease expires while it waits for
user input. The progress message extends only that request's lease; it does
not send a verdict or change policy.

Protocol v4 negotiates this feature during HELLO.

User approval
-------------

Selected events can be handed to an unprivileged desktop approval agent:

```sh
tools/medusa-approval-agent.py \
  --socket "$XDG_RUNTIME_DIR/medusa-approval.sock" \
  --state "$XDG_CONFIG_HOME/medusa/approvals.json"

constable/constable \
  --approval-socket /run/user/1000/medusa-approval.sock \
  --approval-events socket_connect_access,socket_bind_access \
  --approval-uid 1000
```

The same settings belong declaratively in the outer `constable.conf` (not in
the Medusa policy handlers):

```text
approval socket "/run/user/1000/medusa-approval.sock"
    uid 1000
    events "socket_connect_access,socket_bind_access"
    timeout 60;
```

Command-line approval options override this stanza.

The popup offers Allow, Deny, and a “Remember for this event” checkbox.
Remembered choices live in the user-owned JSON state file and can be removed
with `medusa-approval-agent.py --socket PATH --state FILE --clear`. Constable
authenticates both the socket file and peer UID, renews the kernel request
lease while waiting, and denies if the agent is unavailable or its response is
invalid. The Unix socket is the request/response transport; inotify is not
used because watched files do not authenticate a responder and are prone to
replacement and ordering races.

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

## Adaptive generations

The migrated server negotiates protocol-v4 atomic policy replacement and
reply-bound cache updates. Sending `SIGUSR1` to a running Constable asks every
connected Medusa endpoint to stage the complete currently configured fallback
set as the next generation. The kernel keeps the parent active until commit,
cancels parent-generation pending requests at publication, and invalidates
monitored kernel contexts without disconnecting Constable.

An allowed reply may identify the event's monitored subject or object for a
simplified kernel-cache update. Denials never carry this optimization.
Arbitrary context mutations continue through the validated object-update
exchange.

Mining, review, authenticated distribution, monotonic-emergency checks, and
rollback prevention remain outside Constable's transport layer. They are
implemented by the preserved policy-mining hub's Phase 7 userspace workflow.
