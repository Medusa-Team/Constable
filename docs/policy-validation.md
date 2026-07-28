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
