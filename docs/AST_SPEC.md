# AST Node Types (sketch)

This is the pre-code sketch of the AST that Phase 1's parser will build, one node type per grammar rule in `docs/grammar.md`. Written before the parser so parsing logic has a fixed target to construct, per `docs/COMPILER_DESIGN_GUIDE.md`'s "build AST node types before writing parsing logic" step.

This is a **shape sketch, not final C code** — field names/types may shift slightly once the lexer's token representation is locked in, but the node *kinds* and what each one holds shouldn't need to change.

---

## Node kinds

Every node carries a `line` and `col` (from its first token) for error reporting — this is what lets `E2xx`/`E3xx` errors point at exact source locations later, not just the lexer/parser's `E0xx`/`E1xx`.

```
NodeKind:
  NODE_PROGRAM
  NODE_IF
  NODE_FOR
  NODE_WHILE
  NODE_REPEAT
  NODE_FUNCTION_DECL
  NODE_RETURN
  NODE_INPUT
  NODE_OUTPUT
  NODE_ASSIGN
  NODE_CALL
  NODE_BINARY_OP      # +, -, *, /, %, ==, !=, <, >, <=, >=, and, or
  NODE_UNARY_OP       # not, unary -
  NODE_INDEX          # array_name[expr]
  NODE_MEMBER         # array_name.length
  NODE_IDENT
  NODE_NUMBER_LIT
  NODE_STRING_LIT
  NODE_BOOL_LIT
```

## Node shapes

```
Program
  statements: [Node]              # top-level statement list

If
  condition: Node                 # expression
  then_body: [Node]
  else_body: [Node] | null        # null if no else clause

For
  var_name: string
  start: Node                     # expression
  end: Node                       # expression
  body: [Node]

While
  condition: Node
  body: [Node]

Repeat
  body: [Node]
  condition: Node                 # until-condition; checked *after* body

FunctionDecl
  name: string
  params: [string]
  body: [Node]

Return
  value: Node | null              # null for bare `return` with no value

Input
  var_name: string

Output
  value: Node                     # expression to print

Assign
  target: Node                    # Ident or Index (lvalue)
  value: Node                     # expression

Call
  callee: string
  args: [Node]

BinaryOp
  op: TokenType                   # T_PLUS, T_AND, T_EQ, etc.
  left: Node
  right: Node

UnaryOp
  op: TokenType                   # T_NOT or T_MINUS
  operand: Node

Index
  array: Node                     # usually Ident
  index: Node                     # expression

Member
  object: Node                    # usually Ident
  name: string                    # only "length" is valid in v1

Ident
  name: string

NumberLit
  value: double                   # ints and floats both stored as double;
                                   # int-vs-float distinction is a semantic-
                                   # analysis concern, not a parse-time one

StringLit
  value: string

BoolLit
  value: bool
```

## Ownership notes (decide before coding, per the design guide's memory-ownership rule)

- The AST owns all its child nodes and any heap-allocated strings (`var_name`, identifier text, string literals) it copies out of tokens.
- Tokens themselves are transient — once a node has copied what it needs from a token, the token can be discarded. The parser should not keep raw `Token*` pointers inside AST nodes.
- One recursive `free_node(Node*)` walks the tree and frees children before freeing the node itself — mirrors how the tree was built, so there's no separate "how do I free this" design needed per node kind.

## Why doubles for all numbers

Keeping a single `NumberLit` node with a `double` (rather than separate int/float literal nodes) avoids duplicating every arithmetic rule in the parser and semantic analyzer for two literal kinds. Whether a value is treated as an integer (e.g. for loop counters, array indices) is enforced in Phase 1 semantic analysis, not baked into the AST shape.
