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

The compiled-tree corpus is a parser/compiler and namespace-semantics gate.
Runtime handler equivalence remains a separate Phase 4 requirement and must be
tested by executing handlers with controlled event objects.
