# Project Plan — Pseudo Is All You Need

This is the master working plan for the project. It breaks the build into phases, says what "done" means for each, what order things happen in, and why. `README.md` explains *what* the project is; this file explains *how we get there*. `PROJECT_LOG.md` tracks what's actually changed as we go.

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

## Phase 1 — Frontend: Lexer, Parser, AST, Semantic Analysis *(done)*

**Goal:** turn a `.pseudo` source file into a type-checked AST.

1. **Lexer** (`lexer.h` / `lexer.c`)
   - Decided token ownership (`Token` non-owning slices), case-sensitivity, lookahead/peek.
   - Tested on `tests/lex_all_tokens.pseudo`.
2. **Parser** (recursive descent)
   - Precedence climbing (8 levels), AST node construction, panic-mode error recovery across statement boundaries.
   - Tested on `tests/parse_valid.pseudo` and `tests/parse_errors.pseudo`.
3. **Semantic analysis**
   - Scoped symbol tables, forward reference function registration, type and arity checking (`[E2xx]`).
   - Tested on `tests/semantic_errors.pseudo`.

**Exit criteria:** a `.pseudo` file can be lexed, parsed, and semantically checked, with useful multi-error output on bad input. **Met — Phase 1 is complete.**

---

## Phase 2 — Shared IR *(done)*

**Goal:** one IR that both backends consume, built from the Phase 1 AST.

- [x] Finalize linear Three-Address Code instruction set (`include/ir.h`, `src/ir.c`).
- [x] Implement AST $\rightarrow$ IR lowering (`src/ir_gen.c`), flattening expressions and control flow.
- [x] Test: `--dump-ir` inspectable on all valid test programs.

**Exit criteria:** every valid test program produces IR; IR is inspectable/printable for debugging. **Met — Phase 2 is complete.**

---

## Phase 3 — Bytecode VM backend *(done)*

**Goal:** IR runs end-to-end through an interpreter.

- [x] Runtime tagged union values and dynamic heap objects (`include/value.h`, `src/value.c`).
- [x] Bytecode chunk representation, opcodes, and disassembler (`include/chunk.h`, `src/chunk.c`).
- [x] IR $\rightarrow$ Bytecode compiler with two-pass label backpatching (`include/compiler.h`, `src/compiler.c`).
- [x] Stack-based VM execution engine with isolated call frames (`include/vm.h`, `src/vm.c`).
- [x] Tested on `tests/vm_factorial.pseudo` (recursion and loops) and `tests/vm_arrays_loops.pseudo` (arrays, while, repeat-until, string concatenation).

**Exit criteria:** full test programs run correctly end-to-end on the VM. **Met — Phase 3 is complete.**

---

## Phase 4 — AOT-to-C backend *(done)*

**Goal:** the same IR compiles to a native binary.

- [x] Standalone C11 runtime support engine (`include/pseudo_runtime.h`).
- [x] IR $\rightarrow$ C code generator (`include/codegen_c.h`, `src/codegen_c.c`).
- [x] Build pipeline: `pseudoc build <file.pseudo> -o <bin>` (generates `.c` and compiles via host `cc -O2`).
- [x] Tested on `tests/vm_factorial.pseudo` and `tests/vm_arrays_loops.pseudo`. Verified that native AOT binary output matches VM output 100% byte-for-byte.

**Exit criteria:** VM output and AOT-to-C output match, byte-for-byte, on every test program. **Met — Phase 4 is complete.**

---

## Phase 5 — Error handling, logging, tooling *(done)*

**Goal:** the compiler is pleasant to actually use, and produces the training corpus Phase 6 depends on.

- [x] Error log format finalized and wired through all phases: `[Exxx] [Phase] line L, col C: message` $\rightarrow$ `.errlog` file (`include/errlog.h`, `src/errlog.c`).
- [x] Never-stop-at-first-error behavior confirmed across lexer, parser, and semantic analyzer passes.
- [x] CLI polished: `run`, `build`, `repl`, `--check`, `--dump-tokens`, `--dump-ast`, `--dump-ir`, `--dump-c`.
- [x] Interactive Terminal REPL (`include/repl.h`, `src/repl.c`) supporting multi-line blocks and persistent VM session state.
- [x] Full test suite across both backends and error logger (`make test`).

**Exit criteria:** a user can `run`/`build`/`repl` a `.pseudo` program, get multi-error output on bad input, and every session's errors land in `.errlog`. **Met — Phase 5 is complete.**

---

## Phase 6 — ML layer: Adaptive Input Understanding *(done)*

**Goal:** the compiler learns personal typing/typo habits and corrects broken pseudocode in context.

- [x] SQLite error corpus database and pattern tracker (`ml/corpus.py`, `ml/corpus.db`).
- [x] Context-aware Adaptive Rectifier (`ml/rectifier.py`) combining Levenshtein edit distance, parse-position grammar validity, and personal typo frequency priors.
- [x] Automatic error diagnosis and code repair (`--fix`), verified on `tests/ml_broken_sample.pseudo`.
- [x] Master live demonstration script (`demo.sh`) demonstrating all 6 compiler engineering and ML phases.

**Exit criteria:** broken pseudocode containing multiple realistic typos is automatically identified, ranked, repaired, compiled, and executed successfully. **Met — Phase 6 is complete.**

---

## Overall Status Summary

| Phase | Title | Status |
| :-: | :- | :-: |
| 0 | Design & Grammar Specification | **Done** |
| 1 | Frontend (Lexer, Parser, AST, Semantics) | **Done** |
| 2 | Shared Intermediate Representation (IR) | **Done** |
| 3 | Bytecode Virtual Machine Backend | **Done** |
| 4 | Ahead-Of-Time (AOT) C11 Backend | **Done** |
| 5 | Error Logging, Diagnostics, CLI & REPL | **Done** |
| 6 | ML Adaptive Input Understanding & Live Demo Suite | **Done** |

**All 6 project milestones are 100% complete, tested, and verified.**
