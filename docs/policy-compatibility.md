# Preserved policy compatibility

Phase 4 preserves policy behavior by testing historical source files in place.
The test does not maintain rewritten copies that could drift away from the
policies users actually received.

## Compiled compatibility corpus

`constable/tests/historical_policy_test.sh` compiles these policies against the
offline protocol-v3 schema and checks a golden SHA-256 digest of Constable's
resulting virtual-space tree:

| Policy | Scope |
| --- | --- |
| `constable/medusa.conf` | Compiler control flow and the primary file tree |
| `constable/minimal/medusa.conf` | Full minimal authorization policy |
| `constable/minimal/minimal.conf` | Minimal base policy |
| `constable/minimal/y.conf` | Minimal IPC-oriented policy |
| `constable/etc/medusa.conf` | Modular Slackware policy and all nine active `.medusa` includes |

The modular policy exercises more than successful parsing: its 406-line tree
freezes space membership, access masks, primary spaces, regular-expression
paths, and handler placement. A semantic change must update the manifest
deliberately and explain why.

## Migration-only artifacts

The files below are preserved as historical migration inputs, not claimed to
compile with the current grammar:

- `constable/Examples/medusa.conf` and `.example1` predate required tree and
  assignment syntax.
- `constable/test/m1.conf` and `m2.0.conf` use the older slash-led tree grammar;
  `m1.conf` also invokes the retired force-code loader.
- `constable/test/m2.conf` depends on an old include layout and earlier grammar.

These files must not be silently normalized and presented as compatibility
evidence. Porting them requires an explicit grammar migration with expected
semantic output.

The `-T` offline self-test mode executes a policy's `_debug` function through
the real bytecode interpreter and treats only `FORCE_ALLOW` as success.
`policy_runtime_test.sh` freezes compiled function calls, arguments, loops,
switches, return values, and both passing and denying outcomes.

## Controlled event-object execution

The `-E <comm>` offline mode registers a bounded `_debug_event` schema and
executes policy handlers through the production `do_event()` path. Its fixture
freezes:

- source registration order across decision and notification callbacks;
- answer composition for `ALLOW`, `FORCE_ALLOW`, `FAKE_ALLOW`, and `DENY`;
- subject and object mutations made by compiled bytecode;
- `VS_ALLOW` versus `VS_DENY` notification-list routing; and
- rejection of a missing test event or communication interface.

The `-H <comm>` mode goes beyond the synthetic compatibility policy. It loads
the preserved modular Slackware policy, supplies minimal `file`, `process`, and
`getfile` layouts, and replays the real hierarchy callbacks for `/bin/ping`.
The golden result requires the generic tree callback to advance `/` to `/bin`
and then `/bin/ping` before the historical file-capability callback sets only
`CAP_NET_RAW` in the file's `pcap` bitmap. A sibling `/bin/other` replay must
leave `pcap` clear.

Both modes are offline: their schemas and objects exist only inside the
Constable process, no device is opened, and no kernel state is read or
modified. The schema is deliberately limited to fields exercised by these
goldens; it is not presented as a complete current-kernel ABI.

The Linux 7.1 Phase 4 kernel renames the experimental hard-link destination
object class to `path_guard`. The minimal policy uses that descriptive class
for its disabled fetch/update examples, and the historical-policy test requires
those declarations while preserving the policy's compiled-tree digest.
