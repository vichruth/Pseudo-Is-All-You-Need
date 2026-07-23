# Grammar

## Overview

This document is the formal grammar spec for the pseudocode language. It's the single source of truth for the lexer's token set and the parser's rules — every `parse_*()` function in Phase 1 maps to a rule in the "Formal grammar" section below, one function per rule.

Textbook/DAA-style pseudocode is intentionally loose (different courses write `<-` vs `=`, `print` vs `output`, etc.). This spec picks one concrete syntax so the lexer and parser have something unambiguous to implement — see "Design choices" at the bottom for why each choice was made.

---

## Lexical grammar (token types)

These are the token kinds the lexer must emit. Every keyword is case-insensitive (`IF`, `if`, `If` all lex to the same `T_IF` token) — see "Design choices."

| Token | Matches | Example |
| --- | --- | --- |
| `T_IF` / `T_THEN` / `T_ELSE` / `T_ENDIF` | keywords | `if`, `then`, `else`, `endif` |
| `T_FOR` / `T_TO` / `T_DO` / `T_ENDFOR` | keywords | `for`, `to`, `do`, `endfor` |
| `T_WHILE` / `T_ENDWHILE` | keywords | `while`, `endwhile` |
| `T_REPEAT` / `T_UNTIL` | keywords | `repeat`, `until` |
| `T_FUNCTION` / `T_ENDFUNCTION` / `T_RETURN` | keywords | `function`, `endfunction`, `return` |
| `T_INPUT` / `T_OUTPUT` | keywords | `input`, `output` |
| `T_AND` / `T_OR` / `T_NOT` | keywords | `and`, `or`, `not` |
| `T_TRUE` / `T_FALSE` | keywords | `true`, `false` |
| `T_IDENT` | identifier | `total`, `array_name`, `i` |
| `T_NUMBER` | integer or float literal | `42`, `3.14` |
| `T_STRING` | double-quoted string literal | `"hello"` |
| `T_PLUS` `T_MINUS` `T_STAR` `T_SLASH` `T_MOD` | arithmetic operators | `+ - * / %` |
| `T_ASSIGN` | assignment | `=` |
| `T_EQ` `T_NEQ` `T_LT` `T_GT` `T_LTE` `T_GTE` | comparison operators | `== != < > <= >=` |
| `T_LPAREN` `T_RPAREN` | parentheses | `( )` |
| `T_LBRACKET` `T_RBRACKET` | array indexing | `[ ]` |
| `T_COMMA` | argument/parameter separator | `,` |
| `T_DOT` | member access (`.length`) | `.` |
| `T_EOF` | end of input | — |

**Whitespace and comments are not tokens** — the lexer consumes and discards them; they never reach the parser.

### Comments

```plaintext
// this is a single-line comment, runs to end of line
```

Only single-line comments (`//`) are supported in v1. No block-comment syntax — keeps the lexer simpler, and textbook pseudocode rarely needs block comments.

---

## Formal grammar (EBNF-ish)

Notation: `[X]` = optional, `{X}` = zero or more, `|` = alternatives, `UPPER` = literal keyword/token.

```
program        := { statement }

statement      := if_stmt
                | for_stmt
                | while_stmt
                | repeat_stmt
                | function_decl
                | return_stmt
                | io_stmt
                | assignment
                | expr_stmt

if_stmt        := IF "(" expression ")" THEN
                     { statement }
                   [ ELSE
                     { statement } ]
                   ENDIF

for_stmt       := FOR IDENT "=" expression TO expression DO
                     { statement }
                   ENDFOR

while_stmt     := WHILE "(" expression ")" DO
                     { statement }
                   ENDWHILE

repeat_stmt    := REPEAT
                     { statement }
                   UNTIL "(" expression ")"

function_decl  := FUNCTION IDENT "(" [ param_list ] ")"
                     { statement }
                   ENDFUNCTION

param_list     := IDENT { "," IDENT }

return_stmt    := RETURN [ expression ]

io_stmt        := INPUT IDENT
                | OUTPUT expression

assignment     := lvalue "=" expression

lvalue         := IDENT [ "[" expression "]" ]

expr_stmt      := call_expr                      # a bare function call used as a statement

# Expression grammar, lowest to highest precedence
expression     := or_expr

or_expr        := and_expr   { OR  and_expr }
and_expr       := not_expr   { AND not_expr }
not_expr       := [ NOT ] comparison

comparison     := additive   [ ("==" | "!=" | "<" | ">" | "<=" | ">=") additive ]

additive       := multiplicative { ("+" | "-") multiplicative }
multiplicative := unary { ("*" | "/" | "%") unary }
unary          := [ "-" ] postfix

postfix        := primary { "[" expression "]" | "." IDENT }

primary         := NUMBER
                | STRING
                | TRUE | FALSE
                | IDENT
                | call_expr
                | "(" expression ")"

call_expr      := IDENT "(" [ arg_list ] ")"
arg_list       := expression { "," expression }
```

### Precedence table (lowest to highest)

| Precedence | Operators | Associativity |
| :-: | --- | :-: |
| 1 (lowest) | `or` | left |
| 2 | `and` | left |
| 3 | `not` (unary) | — |
| 4 | `==` `!=` `<` `>` `<=` `>=` | non-associative (no chaining: `a < b < c` is not valid) |
| 5 | `+` `-` (binary) | left |
| 6 | `*` `/` `%` | left |
| 7 | `-` (unary negation) | — |
| 8 (highest) | `[...]` indexing, `.` member access, function call `(...)` | left |

This precedence climb is exactly what the recursive-descent parser's function-call chain implements: `or_expr` calls `and_expr` calls `not_expr` ... down to `primary`. Each grammar rule above becomes one `parse_*()` function in the parser (Phase 1, Step 3).

---

## Statement reference

### IF-THEN-ELSE

```plaintext
if (condition) then
    statement_list
[else
    statement_list]
endif
```

### FOR loop

```plaintext
for variable = start to end do
    statement_list
endfor
```

`variable` is implicitly declared as an integer loop counter scoped to the loop body, incrementing by 1 each iteration from `start` to `end` inclusive.

### WHILE loop

```plaintext
while (condition) do
    statement_list
endwhile
```

### REPEAT-UNTIL loop

```plaintext
repeat
    statement_list
until (condition)
```

Body always executes at least once; loop exits when `condition` becomes true (this is the standard textbook semantics — opposite of `while`, which exits when the condition becomes *false*).

### FUNCTION declaration

```plaintext
function name(parameter_list)
    statement_list
    return expression   # optional; a function with no return is called for side effects only
endfunction
```

### Arrays

```plaintext
array_name[index] = value          # element assignment
index = array_name.length - 1      # .length is the only built-in array member
```

Arrays are zero-indexed. Bounds are not fixed at parse time — bounds checking is a semantic/runtime concern (Phase 1 semantic analysis + Phase 3/4 runtime), not a grammar concern.

### I/O

```plaintext
input x           # reads one value into variable x
output expression  # prints the value of expression
```

---

## Semantic rules

(Enforced in Phase 1 semantic analysis, not by the grammar/parser — the parser accepts anything grammatically well-formed; these rules reject grammatically-valid-but-meaningless programs.)

- Conditions (`if`/`while`/`until`) must evaluate to boolean values.
- `for` loop variables and bounds must be integers.
- Arrays must be indexed with in-range integer values; out-of-range access is a runtime error, not a compile-time one (bounds aren't always statically known).
- Function calls must pass exactly the number of arguments the function declares.
- A variable must be declared (assigned at least once) before it is used in an expression — this is the semantic analysis pass's main job (see `E2xx` errors in `README.md`).

---

## Design choices

Why this file picks concrete syntax instead of staying abstract "textbook" pseudocode:

- **`=` for both assignment and comparison-in-declaration-position is avoided** — assignment is `=`, equality comparison is `==`, matching C/most real languages, so the parser and the eventual C output (AOT backend) don't need a translation step for this operator.
- **Keywords are case-insensitive** — textbook pseudocode is inconsistently cased across sources (`IF` vs `if`), and rejecting one casing would be a pointless source of `E0xx` errors. The lexer lowercases keyword text before matching against the keyword table.
- **No semicolons, whitespace-insensitive** — blocks are delimited by keywords (`endif`, `endfor`, ...), not braces or indentation, so statement separation doesn't need an explicit terminator. This matches how textbook pseudocode is actually written.
- **Single-line comments only (`//`)** — kept minimal for v1; block comments can be added later without breaking anything already written, since it's a pure lexer-level addition.
