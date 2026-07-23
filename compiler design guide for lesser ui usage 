# Compiler Design — Working Guide

## Philosophy

Write it yourself first. Use AI (or anyone) to review, not to generate.
Every phase below: read the primary source → write your own version →
get it reviewed → understand *why* any correction is correct before
applying it.

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

## Phase 1 — Grammar & Front-end

### Step 1: Grammar design
- [x] `docs/grammar.md` — done, covers IF/THEN/ELSE, FOR, WHILE, REPEAT/UNTIL,
      FUNCTIONs, arrays, I/O, comments, AND/OR/NOT with precedence

### Step 2: Lexer
- Write `lexer.h` / `lexer.c` yourself, referencing Crafting Interpreters'
  `scanner.c` chapter directly
- Decide explicitly, in writing, before coding:
  - Memory ownership: who allocates `Token.text`, who frees it, when
  - Case sensitivity for keywords
  - Whether you need a lookahead/peek buffer, and exactly how it works
- Test: write a `.pseudo` file with every token type in your grammar,
  run it through the lexer, manually verify every token printed is correct

### Step 3: Parser (recursive descent)
- One function per grammar rule (`parse_if_statement()`, `parse_expression()`,
  etc.) — this maps directly from `docs/grammar.md`, rule by rule
- Build the AST node types before writing parsing logic
- Test against small valid programs *and* deliberately malformed ones —
  check your error messages are actually useful

### Step 4: AST → IR lowering
- Decide your IR's shape before writing the lowering code — sketch it on
  paper first. This is the piece hardest to redo later if wrong.

---

## Phase 2 — Bytecode VM backend

- IR → bytecode compiler
- Stack-based VM interpreter (Crafting Interpreters' `clox` VM is the
  direct reference here)
- Test: same `.pseudo` programs from Phase 1, verify bytecode execution
  produces correct output

## Phase 3 — AOT-to-C backend

- IR → C code generator
- Shell out to `gcc` for the actual native compilation
- Test: same test programs again — both backends must produce identical
  output for identical input. This cross-check is your best correctness
  signal.

## Phase 4 — Tooling & polish

- Error messages with source location (you already tracked line numbers
  in the lexer — use them here)
- CLI: `run`, `build`, `repl`
- Full test suite across both backends

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
