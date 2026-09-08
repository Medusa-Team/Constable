# Constable Policy Language

This document specifies the policy language implemented by the protocol-v3
Constable reference server. It describes existing behavior; it is not a
proposal for protocol v4. The executable semantic fixtures under
`constable/tests/` are normative where prose and implementation disagree.

The compiler is table-driven. Its source grammar is split across
`language/conf_lang.c`, `language/language.c`, `language/data.c`, and
`language/expression.c`. Object classes, event names, attributes, and some
constants are added to the lexer dynamically from the connected kernel.
Consequently, parsing a complete policy requires the schema announced by that
kernel session.

## Lexical form

Identifiers contain ASCII letters, digits, and underscores and may not start
with a digit. Integer constants use the C-style forms accepted by `strtol`
with base zero. Strings and character literals support backslash escapes.
Paths are introduced by `@`; both `@path` and `@"path"` forms are accepted.
Whitespace is insignificant.

The lexer accepts block comments (`/* ... */`) and line comments introduced by
`//` or `#`.

Reserved answer constants are:

| Policy name | Numeric value | Meaning inside Constable |
| --- | ---: | --- |
| `FORCE_ALLOW` | 0 | Allow that may replace an accumulated ordinary allow |
| `DENY` | 1 | Deny; absorbing during handler composition |
| `FAKE_ALLOW` | 2 | Skip/fake allow; overridden only by a later deny |
| `ALLOW` | 3 | Ordinary allow and the implicit handler return |

`RESULT_ERR` (-1) and `RESULT_RETRY` (4) are internal values and are not
policy-language constants.

The event-handler list names are `VS_ALLOW`, `VS_DENY`, `NOTIFY_ALLOW`, and
`NOTIFY_DENY`. The default is `VS_ALLOW`.

Access-relation keywords are `MEMBER`, `READ`/`RECEIVE`, `WRITE`/`SEND`,
`SEE`, `CREATE`, `ERASE`, `ENTER`, and `CONTROL`.

## Top-level grammar

The following EBNF is descriptive. Literal words and punctuation appear in
quotes; `identifier`, `string`, `path`, `expression`, and `commands` are
nonterminals.

```text
policy          = { declaration } ;

declaration     = function-declaration
                | function-definition
                | tree-definition
                | primary-tree
                | space-definition
                | handler-definition
                | access-definition ;

function-declaration
                = "function" identifier ";" ;
function-definition
                = "function" identifier "{" commands "}" ;

tree-definition = "tree" string { identifier } "of" identifier
                  [ "by" identifier expression ] ";" ;
primary-tree    = "primary" "tree" string ";" ;

space-definition
                = [ "primary" ] "space" identifier
                  ( ";"
                  | "=" target-list
                  | ("+" | "-") target-list ) ;
target-list     = target { [","] ("+" | "-") target } ";" ;
target          = [ "-" | "+" ] [ "recursive" ]
                  ( string | path | "space" identifier ) ;

handler-definition
                = subject identifier [ ":" handler-list ] [ object ]
                  "{" commands "}" ;
subject         = selector ;
object          = selector ;
selector        = "*"
                | identifier
                | string
                | path
                | "recursive" (string | path) ;
handler-list    = "VS_ALLOW" | "VS_DENY"
                | "NOTIFY_ALLOW" | "NOTIFY_DENY" ;

access-definition
                = identifier access-item { "," access-item } ";" ;
access-item     = [ access-keyword ] selector ;
```

Tree flags are parsed as identifiers. The current implementation recognizes
only `test_enter` and `clone`; any other flag is an error. The class following
`of` must have been announced by the kernel. A `by` clause names an event and
an expression used to select the tree node.

A space target without a leading sign is inclusive. `-` excludes it and `+`
explicitly includes it. `recursive` applies to a path or referenced space.
`primary space` creates a primary virtual space; `primary tree` selects the
default path tree.

An event selector may refer to a named space, a literal path, a recursive path,
or all objects (`*`). An omitted object selector is distinct from `*` and is
valid only for an event whose kernel schema has no object argument.

## Commands and values

Function and handler bodies accept:

```text
if (expression) command [ else command ]
while (expression) command [ else command ]
do command while (expression) command [ else command ]
for ([expression]; [expression]; [expression]) command
switch (expression) { case expression: commands ... default: commands }
break ;
continue ;
return [expression] ;
{ commands }
expression ;
;
```

The actual grammar accepts its expression nonterminal without requiring
parentheses in several constructs. Existing policies should retain their
current spelling; changing this permissiveness requires a language-version
decision and updated fixtures.

Values include integers, strings, paths, function arguments (`$0`, `$1`, ...),
variables, object attributes (`object.attribute`), function and built-in calls,
`fetch identifier`, `update identifier`, `typeof(expression)`,
`commof(expression)`, `local` variables, and `transparent` objects.

Every function or handler receives an implicit `return ALLOW` at the end.
`return;` has the same result. A handler invocation initializes its candidate
answer to `ALLOW` before executing bytecode.

Expression precedence, highest to lowest, is:

1. primary values, calls, attribute access, and parentheses;
2. unary `!`, `~`, `-`, prefix/postfix `++` and `--`;
3. `*`, `/`, `%`;
4. `+`, `-`;
5. `<<`, `>>`;
6. `<`, `<=`, `>`, `>=`;
7. `==`, `!=`;
8. bitwise `&`;
9. bitwise `^`;
10. bitwise `|`;
11. logical `&&`;
12. logical `^^`;
13. logical `||`;
14. conditional `?:`;
15. assignment and compound assignment;
16. comma.

Binary operators through logical OR are left-associative. Conditional and
assignment operators are right-associative. `&&` and `||` short-circuit.

## Handler matching and order

Handlers are appended to their registration list in policy source order.
For one event and one handler list, Constable evaluates matching handlers in
this order:

1. handlers indexed globally by event name and virtual-space selectors;
2. handlers attached to the subject's matching path-tree nodes;
3. handlers attached to the object's matching path-tree nodes.

Within each list, source registration order is preserved. Subject and object
path-tree traversal follows the registered class-handler and tree traversal
order; policies must not rely on an undocumented order between different
class handlers.

A handler whose virtual-space selector does not match is skipped. A completed
handler participates in answer composition. A handler that returns an
execution error is ignored and evaluation continues with the previously
accumulated answer. An asynchronous handler resumes at the same handler and
does not contribute a candidate answer until it completes.

If no handler completes, the event is reported as unhandled.

## Multi-handler answer composition

The accumulated answer starts as `RESULT_ERR`. Each completed handler's result
is folded into it using the following golden table:

| Accumulated \ candidate | `FORCE_ALLOW` | `DENY` | `FAKE_ALLOW` | `ALLOW` |
| --- | --- | --- | --- | --- |
| `RESULT_ERR` | `FORCE_ALLOW` | `DENY` | `FAKE_ALLOW` | `ALLOW` |
| `FORCE_ALLOW` | `FORCE_ALLOW` | `DENY` | `FAKE_ALLOW` | `FORCE_ALLOW` |
| `DENY` | `DENY` | `DENY` | `DENY` | `DENY` |
| `FAKE_ALLOW` | `FAKE_ALLOW` | `DENY` | `FAKE_ALLOW` | `FAKE_ALLOW` |
| `ALLOW` | `FORCE_ALLOW` | `DENY` | `FAKE_ALLOW` | `ALLOW` |

Any candidate other than the four public answer constants is normalized to
`DENY`. In particular, accidentally returning an internal error or retry value
from a completed handler denies the event.

For an initial `VS_ALLOW` evaluation, `ALLOW` and `FORCE_ALLOW` select
`NOTIFY_ALLOW`; `DENY` and `FAKE_ALLOW` select `NOTIFY_DENY`. For an initial
`VS_DENY` evaluation, all normal completed answers select `NOTIFY_DENY`; the
historical code does not let `FORCE_ALLOW` turn that phase into an allow
notification.

Notification handlers use the same matching and composition machinery. Their
side effects run after the initial decision phase; they do not change which
notification list was selected.

## Schema-dependent validation

The policy compiler may initially retain handlers by symbolic event name
before all byte layouts are known. Once the kernel announces its classes and
events, Constable validates:

- that the event exists on the connection;
- that its subject and object arity matches the rule;
- that path-tree subject/object classes match the announced event operands;
- that referenced object attributes exist in the announced layouts.

Protocol-v3 does not distinguish an announced enforcing event from an
announced but unwired event. Phase 4's validation mode must add that capability
check before a policy is presented as enforceable.

## Integer arithmetic failures

Integer operations use 32 or 64 bits, extending narrower operands to the
operation width. Comparisons always return a 32-bit unsigned Boolean (0 or 1),
even when comparing 64-bit operands. Subsequent arithmetic extends that Boolean
from its declared width; bytes outside that width are not part of its value.

Addition, subtraction, and multiplication reject mathematical results that do
not fit the declared result type. The historical result types are retained:
addition and multiplication of two unsigned operands return unsigned; subtraction
returns signed; other signed/unsigned combinations return signed. Division and
remainder retain C's usual operand conversions, but reject zero divisors,
signed minimum divided by minus one (including remainder), and results that do
not fit the result type. Shifts reject negative or out-of-range counts. Left
shifts also reject negative inputs and values that would overflow the result.

An invalid binary operation reports a runtime diagnostic, unwinds the handler's
nested calls and local variables, and completes with `DENY`. Statements after
the error are not executed, so a later `return FORCE_ALLOW` cannot hide it.
