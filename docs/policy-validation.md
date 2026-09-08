# Active-kernel policy validation

Constable can validate the object classes and events referenced by a compiled
policy against the inventory exported by a Medusa-enabled kernel:

```sh
constable -V /sys/kernel/security/medusa -c medusa.conf constable.conf
```

The directory passed to `-V` must contain the kernel's read-only `events` and
`classes` securityfs files. Validation runs after policy compilation and
before Constable opens the decision channel.

An event or class passes only when its inventory entry reports
`enforcement=active`. Constable rejects the policy when an entry is missing or
only announced. Announcement-only entries describe protocol-visible surfaces
that the running kernel does not actively enforce, so accepting them would
give a policy author a false assurance.

Validation also fails closed for unreadable files, malformed lines, duplicate
names, and inventory lines longer than 64 KiB. Unknown key-value fields are
ignored so a kernel can extend the inventory without breaking older
validators. Constable prints individual diagnostics followed by event and
class summaries.

The event inventory fields have the following meanings:

- `event` is the stable symbolic policy name; `subject_class` and
  `object_class` name its two operands.
- `event_bit` is the index used by Medusa's trigger bitmap. The reserved
  not-triggered value represents an event that is always evaluated.
- `enforcement` is `active` when a live hook can enforce the event and
  `announced` when only its protocol schema is present.
- `trigger` identifies the operand whose state change invokes the event, or
  `always`; `trigger_bitmap` identifies the operand whose bitmap contains the
  enable bit, or `none` for an always-triggered event.
- `delegation` states whether the hook may sleep while consulting userspace.
- `fallback` is the currently installed degraded-decision policy. The remaining
  counters report evaluation, cache, delegation, verdict-source, timeout, and
  invalid-reply outcomes for operational diagnosis.

For each class, `announced_events` counts registered event schemas that refer to
the class, while `enforced_events` counts the subset backed by active hooks.
These counters explain why a class may be protocol-visible but still report
`enforcement=announced`.

`-V` is compatible with normal operation and with offline `-t`, `-T`, and `-I`
workflows. It does not modify policy or kernel state.

The Phase 6 network fixture validates all supported socket operations:
`socket_create`, `socket_bind_access`, `socket_connect_access`,
`socket_listen_access`, `socket_accept_access`, `socket_sendmsg_access`, and
`socket_recvmsg_access`. A successful validation reports seven active events
and the two referenced active classes, `process` and `socket`. This makes the
network enforcement status visible before Constable opens the decision
channel; an announced-only or absent socket hook fails closed like any other
policy surface.
