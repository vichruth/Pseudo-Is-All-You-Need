# Compiler Design — Working Guide

This file is the *how* for each phase. `PROJECT_PLAN.md` has the authoritative
phase numbers, scope, and exit criteria for the whole project — the phase
headers below match that numbering (Phase 0–6) so the two files don't drift
out of sync.

## Philosophy

Write it yourself first. Use AI (or anyone) to review, not to generate.
Every phase below: read the primary source → write your own version →
get it reviewed → understand *why* any correction is correct before
applying it.

For this project specifically, code written with AI assistance must still
be commented to explain what it does — the point of "AI reviews, doesn't
generate" is that a human understands every line, and comments are how
that understanding gets checked.

---

## Primary references (read these directly, don't rely on summaries)

- **Crafting Interpreters** by Robert Nystrom — free at craftinginterpreters.com.
  Type out the C (`clox`) chapters by hand, don't copy-paste. The friction
  is where retention happens.
- **C11 standard** (or a working draft, freely available as PDF) — for anything
  ambiguous about unions, integer promotion, pointer arithmetic.
- `man malloc`, `man realloc`, `man free` — read these before writing any
  memory-management code, not after something breaks.
- **Engineering a Compiler** (Cooper & Torczon) — heavier, more formal,
  good once Crafting Interpreters' approach feels solid and you want the
  deeper theory (dataflow, optimization, register allocation).

---

## Phase 0 — Design *(done)*

- [x] `docs/grammar.md` — covers IF/THEN/ELSE, FOR, WHILE, REPEAT/UNTIL,
      FUNCTIONs, arrays, I/O, comments, AND/OR/NOT with precedence, plus the
      full lexical token table and formal EBNF grammar
- [x] `docs/AST_SPEC.md` / `docs/IR_SPEC.md` — sketches of the AST node
      shapes and IR instruction set, written before any parser/codegen code
- [x] `README.md`, `PROJECT_PLAN.md` — architecture and phase plan

## Phase 1 — Frontend: Lexer, Parser, Semantic Analysis

### Step 1: Lexer
- Write `lexer.h` / `lexer.c` yourself, referencing Crafting Interpreters'
  `scanner.c` chapter directly
- Decide explicitly, in writing, before coding:
  - Memory ownership: who allocates `Token.text`, who frees it, when
  - Case sensitivity for keywords (see `docs/grammar.md` — keywords are
    case-insensitive by design)
  - Whether you need a lookahead/peek buffer, and exactly how it works
- Test: write a `.pseudo` file with every token type from
  `docs/grammar.md`'s lexical grammar table, run it through the lexer,
  manually verify every token printed is correct

### Step 2: Parser (recursive descent)
- One function per grammar rule (`parse_if_statement()`, `parse_expression()`,
  etc.) — this maps directly from `docs/grammar.md`'s formal grammar, rule
  by rule, including the precedence-climbing chain for expressions
- Build the AST node types from `docs/AST_SPEC.md` before writing parsing
  logic
- Test against small valid programs *and* deliberately malformed ones —
  check your error messages are actually useful

### Step 3: Semantic analysis
- Scope resolution (declared-before-use) and type checking, per the
  "Semantic rules" section of `docs/grammar.md`
- Emits `E2xx` errors in the same format as the lexer/parser's `E0xx`/`E1xx`

---

## Phase 2 — Shared IR

- Finalize the IR shape from `docs/IR_SPEC.md` and write AST → IR lowering
- This is the piece hardest to redo later if wrong — don't skip the sketch
  step, even under time pressure

## Phase 3 — Bytecode VM backend

- IR → bytecode compiler
- Stack-based VM interpreter (Crafting Interpreters' `clox` VM is the
  direct reference here)
- Test: same `.pseudo` programs from Phase 1, verify bytecode execution
  produces correct output

## Phase 4 — AOT-to-C backend

- IR → C code generator
- Shell out to `gcc` for the actual native compilation
- Test: same test programs again — both backends must produce identical
  output for identical input. This cross-check is your best correctness
  signal.

## Phase 5 — Error handling, logging & tooling

- Error messages with source location (you already tracked line numbers
  in the lexer — use them here); `.errlog` format is in `README.md`
- CLI: `run`, `build`, `repl`
- Full test suite across both backends

## Phase 6 — ML layer (deferred)

- Not started until Phase 1–5 produce a real `.errlog`/keystroke corpus —
  see `README.md`'s "ML layer" section for the full design

---

## Review checklist (use this every time you bring code to a reviewer, human or AI)

1. Does this actually solve what I asked, or did it drift to something else?
2. Memory/ownership — who allocates, who frees, when? Any leak or
   double-free risk?
3. Any unused/dead/redundant fields or functions in this design?
4. Did I actually run/test this, or am I just assuming it works?
5. Does the "why" behind any suggested change make sense, or is it vague?
   If vague — go check the primary source (C standard, man pages,
   Crafting Interpreters) before accepting it.

---

## Debugging tools (use these, not just AI conversation)

```bash
cppcheck --enable=all lexer.c
valgrind --leak-check=full ./your_test_binary
gdb ./your_test_binary
```

These catch real bugs mechanically — a genuine second opinion that
doesn't hallucinate.

---

## AI usage rule for this project

- AI reviews code you already wrote. It does not generate the first draft.
- Every AI suggestion gets checked against a primary source before you
  accept it, especially anything about memory management or grammar
  correctness.
- If you don't understand *why* a suggested fix works, don't apply it
  yet — go read the relevant section of Crafting Interpreters or the
  man page first.
