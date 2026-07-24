# Constable protocol v3 baseline

This note describes the reference authorization server at the
`medusa-constable-v3-legacy` tag. It records the existing contract before any
kernel migration or protocol redesign.

## Role

Constable receives the kernel's class and event schema at startup, interprets
policy in userspace, and returns allow or deny decisions. Simple relations can
remain cached in kernel virtual-space and action bitmaps; requests requiring
policy logic are delegated synchronously to Constable.

The server is intentionally policy-programmable and can update registered
kernel objects. That makes network-fed or behaviour-derived policy possible in
principle, but the current repository does not provide a secure network control
plane, authentication model, learning system, or safe rollout mechanism.

## Connection sequence

1. Constable opens the configured Medusa character device.
2. It receives the kernel greeting and uses the version supplied there.
3. The kernel publishes class and event definitions.
4. Constable processes the ready exchange.
5. Authorization requests and fetch/update traffic use the established schema.

Worker threads execute policy events, but the kernel-side transport serializes
the delegated request/answer path. Parallel policy evaluation is therefore not
an end-to-end property of this baseline.

## Wire-format constraints

The matching kernel advertises protocol version 3. The Constable-local
`MEDUSA_COMM_VERSION` macro still says 1, although greeting processing takes the
runtime version from the kernel. This duplication is technical debt.

The protocol also has fixed-size names, packed structures, native-layout object
payloads, and pointer-shaped 64-bit identifiers. Attribute type `0x05` is named
`MED_COMM_TYPE_BYTES` by the kernel but `MED_COMM_TYPE_BITMAP_16` here. These
properties make compatibility fragile across implementations and
architectures. They must be captured by compatibility tests before being
replaced; they should not be copied into a new ABI unchanged.

## Security baseline

Constable is trusted as part of the security mechanism and is exempted from
kernel decisions that would recurse into it. If it is absent or communication
fails, the matching kernel permits requests that reached delegation. Invalid
answers from a reachable server are denied. A hung server can block a delegated
operation.

Remote administration would expand the trusted computing base. A future design
needs authenticated and integrity-protected control messages, authorization for
policy changes, replay protection, bounded timeouts, auditability, atomic
policy generations, and a recovery policy that is explicit rather than
accidental.

## Reproduction scope

Phase 0 treats the existing C implementation and device protocol as a reference
system. It may receive build fixes, tests, and documentation, but no semantic
redesign. The next protocol should be versioned independently and should have
explicit widths, byte order, length-delimited names, request IDs, concurrent
in-flight decisions, capability negotiation, and documented failure handling.
