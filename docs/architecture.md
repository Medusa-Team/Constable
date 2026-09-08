# Constable architecture

Constable is the protocol-v4 reference authorization server for Medusa. This
document maps the implementation boundaries that must remain visible while the
kernel and protocol evolve. The historical protocol-v3 boundary remains
documented separately for compatibility work.

## Startup and configuration

`constable/init.c` owns process startup. Its sequence is:

1. `cli_options.c` parses exact command-line options into
   `struct constable_cli_options`. It performs no I/O or policy mutation.
2. `init_all()` initializes buffer management and the configured MCP
   connections.
3. Each active module announces or imports its schema-dependent namespace.
4. `language/` compiles the selected Medusa policy.
5. `space_apply_all()` resolves virtual-space declarations into concrete,
   deduplicated members and monitoring masks.
6. Offline inspection, validation, and semantic-test modes run and exit, or
   `comm_do()` starts the live worker loop.

The positional configuration file describes Constable communication modules.
`-c` selects the Medusa policy source. Keeping those inputs distinct matters:
the first determines how Constable connects, while the second determines what
the authorization server enforces.

## Runtime layers

| Layer | Primary files | Responsibility |
|---|---|---|
| Process setup | `init.c`, `cli_options.c` | Parse startup state, initialize modules, select offline or live operation |
| Transport and protocol | `mcp/mcp.c`, `mcp/validate.c` | Read and write framed protocol-v4 messages, validate dynamic definitions, correlate request traffic, and install policy generations |
| Connection scheduling | `comm.c`, `comm_buf.c` | Own worker queues, buffers, read/write threads, and resumable execution state |
| Dynamic schema | `class.c`, `object.c`, `event.c` | Represent kernel-announced classes, attributes, events, and decision contexts |
| Policy model | `tree.c`, `space.c`, `vs.c` | Build the unified namespace tree, resolve virtual-space membership, and calculate access masks |
| Policy compiler/runtime | `language/`, `libmcompiler/` | Parse policy source, compile handlers, and execute their bytecode |
| Optional RBAC | `rbac/` | Map users and roles onto the same policy objects and virtual-space model |
| Review tools | `policy_inspect.c`, `policy_validate.c`, `policy_event_test.c` | Inspect without mutation, compare policy references with an active kernel, and replay semantics offline |

Dependencies flow down this table: the protocol supplies schema objects used by
the policy model, and decision execution consumes both. A transport must not
embed policy-language behavior, and a policy handler must not parse wire
frames.

## Connection and decision lifecycle

Protocol v4 starts with feature negotiation, then the kernel dynamically
announces classes, attributes, and events using fixed-width little-endian
frames. Definitions remain connection-local because object layouts are supplied
by that connection. Constable stages a complete policy generation after the
schema is complete, resolves monitoring masks, and executes the optional policy
`_init` handler. Only then does it send READY.

Each event definition carries an explicit kind. Access events produce an
authorization verdict and may have fallback or domain decision policy.
Object-notification events (`get*`) instead let handlers assign context to the
announced object through updates; their reply is only a completion
acknowledgement, so decisional policy is rejected for them.

Wire identifiers are grouped by lifecycle so packet traces remain legible and
each family has room to grow. Message values `1`–`15` are negotiation,
`16`–`31` schema inventory, `32`–`47` policy installation, and `48` onward
runtime traffic; `255` is the generic error. TLV identifiers use the same broad
families: negotiation starts at `1`, schema at `16`, policy and decision values
at `32`, and structured error details at `48`. Gaps are reserved rather than
implicitly reusable. Feature values are independent bit flags because peers
negotiate an arbitrary supported subset.

Configured fallback policies live in `fallback_policy.c`. MCP resolves their
symbolic event names only after the kernel has announced its connection-local
schema. It queues bounded policy frames before READY; policy compilation and
normal decision evaluation do not own this handshake state. The configured set
is allocated atomically and may cover the announced access-event inventory.
Object-notification events remain in the schema but cannot receive fallback
policy.

For a decision:

1. The MCP reader allocates a `comm_buffer_s`, validates the frame, and resolves
   its event definition.
2. `comm.c` places the buffer on the work queue.
3. `event.c:do_event()` materializes operation, subject, and object views and
   runs matching handlers in the documented global/tree order.
4. A handler may complete immediately or suspend while a fetch/update is in
   flight. Its execution stack and phase remain attached to the same buffer.
5. `decision.c` composes handler results; MCP sends the final answer carrying
   the original request identity.

An interactive handler may renew that request's kernel liveness lease. Renewal
does not produce a verdict and must use the same request buffer.

## Ownership and concurrency

- A `comm_s` owns one connection's file descriptor, dynamic class/event
  tables, output queue, read/write threads, and initialization state.
- A `comm_buffer_s` owns one in-flight message plus its variable data,
  execution stack, event context, and asynchronous continuation state.
  Queueing transfers scheduling responsibility; it does not clone the buffer.
- Kernel-announced object data is viewed through the owning message buffer.
  Resizing must rebase every such view; `comm_buffer_test` covers this rule.
- Compiled policy structures and the unified namespace tree are process-lifetime
  state after initialization. Offline inspection must not mutate them.
- Queue locks protect queue links; `comm_s.state_lock` protects connection
  readiness and the `_init` buffer; `comm_s.read_lock` serializes frame reads.
  Handler code must not retain those locks while waiting for userspace work.
- Fetch/update continuations are identified by connection-local class and
  sequence data. They wake the original buffer rather than a shared global
  answer slot.

These rules are the compatibility boundary for refactoring. If ownership moves,
the corresponding focused test must move with it.

## Decision semantics

Handler ordering and result composition are specified in
[`policy-language.md`](policy-language.md). In particular, later handlers still
run after ordinary denial, object mutations are visible to subsequent handlers,
and FORCE_ALLOW has distinct precedence. The semantic corpus—not incidental
control flow in `event.c`—is authoritative when code is reorganized.

The kernel remains responsible for its installed baseline, cache, request
identity, liveness deadline, and degraded behavior. Constable supplies only an
authoritative slow-path answer; a progress message merely proves that one
pending request is alive.

## Safe change workflow

1. Identify the owning layer and keep new state within that layer.
2. Add a focused test under `constable/tests/`.
3. Run `make -C constable test`.
4. Run the full GCC, Clang, AddressSanitizer, and UndefinedBehaviorSanitizer
   jobs described in the repository README.
5. For protocol or policy-semantic changes, also run the matching Linux/QEMU
   scenario and update the protocol or policy documentation.

Protocol-v4 frames remain behind the existing connection/decision boundary.
Compatibility changes must not silently reinterpret historical protocol-v3
native structures.

The event-kind schema extension must be deployed with the matching kernel and
both authorization clients. EVENT_DEFINITION requires the one-byte EVENT_KIND
TLV (28): 0 for access decisions, 1 for object notifications. Updated clients
reject missing or unknown kinds rather than guessing from event names. Older
clients cannot consume this required TLV. Do not mix these revisions during an
upgrade. Default POLICY_EVENT records still carry baseline_allow for schema
completeness; for notifications this is a protocol placeholder, not a configurable
allow/deny fallback. Their security context is assigned by the notification
handler's object updates.
