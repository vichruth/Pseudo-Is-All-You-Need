# Project Plan — Pseudo Is All You Need

This is the working plan for the project. It breaks the build into phases, says what "done" means for each, what order things happen in, and why. `README.md` explains *what* the project is; this file explains *how we get there*. `PROJECT_LOG.md` tracks what's actually changed as we go.

---

## Ground rules (from `docs/COMPILER_DESIGN_GUIDE.md`)

- Write it yourself first. AI/reviewers review — they don't generate the first draft.
- Read the primary source before writing code for that piece (Crafting Interpreters chapter, C11 standard section, `man malloc`).
- No phase starts until the previous one is tested and understood, not just "written."
- Every phase ends with something runnable, even if small.

---

## Phase 0 — Design *(done)*

**Scope:** grammar spec, architecture, AST/IR sketches, error-handling design, this plan, repo scaffolding.

- [x] `docs/grammar.md` — full lexical token table, formal EBNF grammar, precedence table, IF/THEN/ELSE, FOR, WHILE, REPEAT/UNTIL, FUNCTIONs, arrays, I/O, comments, AND/OR/NOT precedence
- [x] `docs/AST_SPEC.md` — AST node shapes for every grammar rule, ownership notes
- [x] `docs/IR_SPEC.md` — shared IR instruction set sketch, AST→IR lowering table, open questions flagged for Phase 2
- [x] `README.md` — architecture, dual-backend rationale, error-log format, ML layer design
- [x] `docs/COMPILER_DESIGN_GUIDE.md` — working method, primary references, review checklist, phase numbers aligned to this file
- [x] `PROJECT_PLAN.md` (this file) + `PROJECT_LOG.md`
- [x] Repo scaffolding — empty `src/`, `include/`, `tests/` directories ready for Phase 1

**Exit criteria:** grammar and architecture reviewed and agreed on before any code is written. **Met — Phase 0 is complete.**

---

## Phase 1 — Frontend: Lexer, Parser, AST, Semantic Analysis

**Goal:** turn a `.pseudo` source file into a type-checked AST.

1. **Lexer** (`lexer.h` / `lexer.c`)
   - Decide, in writing, before coding: token ownership (who allocates/frees `Token.text`), keyword case-sensitivity, lookahead/peek design.
   - Reference: Crafting Interpreters' `scanner.c`, typed out by hand.
   - Test: one `.pseudo` file exercising every token type in the grammar; manually verify every emitted token.

2. **Parser** (recursive descent)
   - One function per grammar rule (`parse_if_statement()`, `parse_for_loop()`, `parse_expression()`, ...), mapped 1:1 from `docs/grammar.md`.
   - Build AST nodes matching the shapes already sketched in `docs/AST_SPEC.md`.
   - Error recovery: synchronize at statement boundaries so one parse pass surfaces multiple `E1xx` errors, not just the first.
   - Test: valid programs *and* deliberately malformed ones — check error messages are actually useful (line + col + "expected X").

3. **Semantic analysis**
   - Scope resolution (variable declared-before-use, function argument counts).
   - Type checking (loop variables are integers, conditions are boolean, array indices are valid).
   - Emits `E2xx` errors, same `.errlog` format as the lexer/parser.

**Exit criteria:** a `.pseudo` file can be lexed, parsed, and semantically checked, with useful multi-error output on bad input, and no IR lowering yet.

---

## Phase 2 — Shared IR

**Goal:** one IR that both backends consume, built from the Phase 1 AST.

- Finalize IR instruction shape from the sketch and open questions in `docs/IR_SPEC.md`.
- Write `AST → IR` lowering, following the lowering table in `docs/IR_SPEC.md`.
- Test: every Phase 1 test program lowers to IR without crashing; spot-check IR by hand for a few representative programs (an `if`, a `for`, a recursive function).

**Exit criteria:** every valid test program produces IR; IR is inspectable/printable for debugging.

---

## Phase 3 — Bytecode VM backend

**Goal:** IR runs end-to-end through an interpreter.

- IR → bytecode compiler.
- Stack-based VM interpreter (direct reference: Crafting Interpreters' `clox` VM).
- Basic standard library / built-ins (I/O, array ops).
- Test: same `.pseudo` programs from Phase 1/2 — verify bytecode execution produces the *correct output*, not just "doesn't crash."

**Exit criteria:** a full test program (with loops, functions, arrays) runs correctly on the VM.

---

## Phase 4 — AOT-to-C backend

**Goal:** the same IR compiles to a native binary.

- IR → C code generator.
- Build pipeline: shell out to `gcc`/`cc`, link a shared runtime support library (used by both backends where possible).
- Test: **same test programs as Phase 3.** Both backends must produce identical output for identical input — this cross-check is the primary correctness signal for the whole project.

**Exit criteria:** VM output and AOT-to-C output match, byte-for-byte, on every test program.

---

## Phase 5 — Error handling, logging, tooling

**Goal:** the compiler is pleasant to actually use, and produces the training corpus Phase 6 depends on.

- Error log format finalized and wired through all phases: `[Exxx] [Phase] line L, col C: message` → `.errlog` file (see `README.md` → Error handling & error logs).
- Never-stop-at-first-error behavior confirmed across lexer/parser/semantic/codegen.
- CLI: `run`, `build`, `repl`.
- Full test suite across both backends (shared test programs, expected-output comparison).

**Exit criteria:** a user can `run`/`build` a `.pseudo` file, get well-formatted multi-error output on bad input, and every session's errors land in `.errlog`.

---

## Phase 6 — ML layer: Adaptive Input Understanding *(deferred)*

**Not started until Phases 1–5 are stable and have produced a real `.errlog` corpus** — this is a hard dependency, not a soft one. Full design already written up in `README.md`. Rough shape when it starts:

1. Keystroke + commit logging pipeline (local, per-user, SQLite-backed).
2. Baseline rectifier: edit-distance against valid tokens in current grammar context.
3. Personalized model: learns from a given user's `.errlog`/keystroke history.
4. Context-aware rectification ranked by edit distance + grammar validity + personal frequency.
5. Suggestion UI: inline, non-blocking, logged as `[Exxx] ... → suggested correction (confidence: X%)`.
6. Evaluation: does personalization measurably reduce real syntax errors over time.

**Exit criteria:** not defined yet — depends on what the Phase 1–5 corpus actually looks like.

---

## Workflow per phase (how we actually work through each one)

1. Read the relevant primary source section first (Crafting Interpreters chapter / C standard section / man page).
2. Write the code by hand.
3. Test against the phase's own test programs before moving on.
4. Review using the checklist in `docs/COMPILER_DESIGN_GUIDE.md` (correctness, memory ownership, dead code, "did I actually run this").
5. Log what changed in `PROJECT_LOG.md`.
6. Only then start the next phase.

## Overall status

| Phase | Status |
| :-: | :-: |
| 0 — Design | **Done** |
| 1 — Frontend | Not started |
| 2 — Shared IR | Not started |
| 3 — Bytecode VM | Not started |
| 4 — AOT-to-C | Not started |
| 5 — Error handling & tooling | Not started |
| 6 — ML layer | Deferred |

Next concrete step: **Phase 1, lexer.**
