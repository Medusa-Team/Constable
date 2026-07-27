# Non-mutating policy inspection

Constable can compile a policy and write a deterministic JSON representation
without opening the decision loop:

```sh
constable -I policy.json -c medusa.conf constable.conf
```

Use `-I -` to write JSON to standard output. Inspection implies test mode, so
Constable exits before communication begins and cannot install or update
kernel policy.

The top-level object has format identifier `constable-policy-v1` and contains:

- `spaces`: each retained virtual space, its stable name, primary/used flags,
  member count, 64-bit identifier, and access sets;
- `namespace`: a flat traversal of compiled paths, primary spaces, access
  sets, and subject/object handler placement;
- `events`: every referenced event and its space-to-space rules;
- `unreachable_rules`: rules whose allocated subject or object space has no
  compiled namespace members.

Anonymous spaces are labelled `?@N`, where `N` is their unique virtual-space
bit. Named spaces retain their policy names. Access sets contain these stable
labels rather than ambiguous display strings.

The document is read-only diagnostic output. It contains no command or field
that can mutate the compiled policy, a process, an inode, or kernel state.
Protocol-schema availability and active-kernel enforcement are intentionally
separate validation dimensions; later validation can annotate this document
without changing its compiled-policy facts.
