# Project Log

Running record of what actually changed in this project, in order. `PROJECT_PLAN.md` is the plan; this is the history. Newest entries at the top.

---

## 2026-08-19 — Phase 6 complete: ML Adaptive Input Understanding & Live Demo Suite

- Added `ml/corpus.py` — manages typo-to-correction patterns and historical diagnostics in a local SQLite database (`ml/corpus.db`). Seeds known pseudocode typo profiles and parses `.errlog` files to track personal frequency distributions.
- Added `ml/rectifier.py` — context-aware error rectification engine. Uses composite confidence ranking across Levenshtein edit-distance ($S_{\text{edit}}$), compiler parse-position grammar validity ($S_{\text{grammar}}$), and personal user frequency priors ($S_{\text{freq}}$). Provides automated auto-fix mode (`--fix`).
- Added `tests/ml_broken_sample.pseudo` — test program with realistic syntax and typing errors (`functon`, `thenn`, `retun`, `fr`, `otput`). Verified that `rectifier.py` identifies and corrects all anomalies, writes `.fixed.pseudo`, and successfully compiles and runs on `pseudoc`.
- Added `demo.sh` — master presentation script supporting interactive step-by-step and automated (`--auto`) modes. Showcases the entire 6-phase compiler and ML pipeline with ANSI-colored outputs, live diff proofs, persistent REPL sessions, and ML auto-repair.
- Verified `./demo.sh --auto` passes all 6 stages with zero errors.

---

## 2026-08-19 — Phase 5 complete: Error Logging, Diagnostics, Tooling & REPL

- Added `include/errlog.h` and `src/errlog.c` — durable error recording and diagnostics system. Emits structured diagnostic messages (`[Exxx] [Phase] line L, col C: message`) to `stderr` and persists them to `.errlog` file for Phase 6 training data collection.
- Integrated `errlog_report` across the entire pipeline: Lexer (`T_ERROR`), Parser (`[E100]` across statement recovery boundaries), and Semantic Analyzer (`[E201]` use before assign, `[E202]` boolean condition, `[E203]` integer range/index, `[E204]` arity mismatch, `[E205]` member access).
- Added `include/repl.h` and `src/repl.c` — interactive Read-Eval-Print Loop terminal session (`pseudoc` / `pseudoc repl`). Features persistent `VM` and `SemanticAnalyzer` scope state across multi-line inputs, live expression evaluation, and command help (`help`, `clear`, `exit`/`quit`).
- Updated `src/main.c` with unified CLI commands: `pseudoc` (REPL), `pseudoc run <file>`, `pseudoc build <file> [-o <bin>]`, `pseudoc --check <file>`, `pseudoc --dump-tokens`, `pseudoc --dump-ast`, `pseudoc --dump-ir`, `pseudoc --dump-c`.
- Updated `Makefile` with `test-errlog` recipe verifying that multi-error recovery correctly persists all diagnostics to `.errlog`.
- All Phase 1, 2, 3, 4, and 5 tests pass cleanly.

---

## 2026-08-19 — Phase 4 complete: AOT-to-C Backend & Native Compilation

- Added `include/pseudo_runtime.h` — lightweight, freestanding C11 runtime header implementing the polymorphic `PseudoValue` tagged union system, auto-expanding dynamic arrays with `.length`, string concatenation, and typed standard I/O.
- Added `include/codegen_c.h` and `src/codegen_c.c` — Ahead-Of-Time (AOT) C code generator. Lowers Three-Address Code IR into clean, standalone C11 source files with forward function declarations, local temporaries, and direct mapping of all control flow, function calls, and array indexing.
- Updated `src/main.c` to add `--dump-c <file.pseudo>` and `build <file.pseudo> [-o <binary>]` (which generates C code and invokes host `cc -O2 ... -lm` to produce native executables).
- Updated `Makefile` with `test-aot` target performing cross-backend verification: compiles `.pseudo` test programs to native AOT binaries, executes them, and diffs output against Bytecode VM output.
- Verified 100% byte-for-byte output equivalence between VM and Native AOT binaries across `tests/vm_factorial.pseudo` and `tests/vm_arrays_loops.pseudo`.

---

## 2026-08-18 — Phase 2 & Phase 3 complete: Shared IR & Bytecode VM Backend

- Added `include/ir.h` and `src/ir.c` — linear Three-Address Code intermediate representation. Defines 3-address instruction set (`IR_CONST`, `IR_LOAD`, `IR_STORE`, `IR_LOAD_IDX`, `IR_STORE_IDX`, `IR_LEN`, `IR_BINOP`, `IR_UNOP`, `IR_LABEL`, `IR_JUMP`, `IR_JUMP_IF_FALSE`, `IR_CALL`, `IR_RETURN`, `IR_INPUT`, `IR_OUTPUT`, `IR_FUNC_BEGIN`, `IR_FUNC_END`), operand structures, list management, memory cleanup, and IR disassembly printing.
- Added `src/ir_gen.c` — AST-to-IR lowering. Lowers expressions to virtual temporary slots (`t0, t1, ...`), control flow (`If`, `For`, `While`, `Repeat-Until`) into explicit label/jump pairs, and functions into bounded function blocks.
- Added `include/value.h` and `src/value.c` — tagged union `Value` system (`VAL_NIL`, `VAL_BOOL`, `VAL_NUMBER`, `VAL_STRING`, `VAL_ARRAY`, `VAL_FUNCTION`) and heap-allocated objects (`ObjString`, `ObjArray` with auto-expanding storage).
- Added `include/chunk.h` and `src/chunk.c` — bytecode chunk format, opcode definitions, constants pool, and bytecode disassembler.
- Added `include/compiler.h` and `src/compiler.c` — IR-to-bytecode compiler. Encapsulates functions into `ObjFunction` chunks, allocates frame-isolated local temporary/parameter slots, and resolves label jumps with two-pass backpatching.
- Added `include/vm.h` and `src/vm.c` — stack-based Virtual Machine execution engine. Features isolated `CallFrame` registers for recursive re-entrancy, globals table, arithmetic/string concatenation, array indexing, and standard I/O.
- Updated `src/main.c` and `Makefile` to support `--dump-ir <file.pseudo>` and `run <file.pseudo>`.
- Added tests: `tests/vm_factorial.pseudo` (recursive factorial and loop accumulation) and `tests/vm_arrays_loops.pseudo` (arrays, while, repeat-until, string concatenation). All Phase 1, 2, and 3 tests pass cleanly.

---

## 2026-08-18 — Phase 1 complete: Parser, AST & Semantic Analysis

- Added `include/ast.h` and `src/ast.c` — complete tagged union AST representation matching `docs/AST_SPEC.md`, with dynamic node lists (`AstNodeList`, `AstStringList`), recursive memory destruction (`ast_free`), and hierarchical tree printing (`ast_print`).
- Added `include/parser.h` and `src/parser.c` — recursive descent parser implementing all grammar rules from `docs/grammar.md`. Features full precedence climbing for expressions (`or` -> `and` -> `not` -> comparisons -> additive -> multiplicative -> unary -> postfix -> primary), 1-token lookahead statement disambiguation for identifiers (`=`/`[` vs `(`), and statement-boundary panic mode synchronization for multi-error recovery (`E1xx`).
- Added `include/semantics.h` and `src/semantics.c` — symbol table and scope resolution pass with type checking. Validates declared/assigned-before-use rules (`E201`), boolean condition types (`E202`), integer loop ranges/array indices (`E203`), function call arity matching (`E204`), and member access (`E205`). Multi-pass architecture registers function signatures globally so mutual and recursive calls work seamlessly.
- Updated `src/main.c` and `Makefile` to support `--dump-tokens`, `--dump-ast`, and `--check`.
- Added tests: `tests/parse_valid.pseudo`, `tests/parse_errors.pseudo`, and `tests/semantic_errors.pseudo`. Verified all passes with clean compilation and accurate multi-error recovery.

---

## 2026-08-04 — Phase 1, Step 1: lexer

- Added `include/token.h`, `include/lexer.h`, `src/lexer.c` — the lexer, **adapted from Crafting Interpreters' `c/scanner.c`** (Robert Nystrom, MIT licensed) rather than written from scratch. Attribution and a list of what was changed are in the header comment of each file. What carried over: the `start`/`current` pointer pair, `advance`/`peek`/`peek_next`/`match`, the `skip_whitespace` switch loop, and the shape of `number()`/`string()`/`identifier()`. What was rewritten: the entire token set (per `docs/grammar.md`), the keyword lookup, column tracking, and the removal of clox's file-scope `Scanner` global.
- Locked the four decisions `docs/COMPILER_DESIGN_GUIDE.md` said to make in writing before coding — they're documented in the `lexer.h` header comment: (1) tokens are non-owning `(start, length)` slices into the source buffer, so the lexer never allocates; (2) case-insensitive keywords are handled by folding into a stack buffer and doing a table lookup, since non-owning tokens can't be lowercased in place and clox's per-character keyword trie is case-sensitive by construction; (3) two characters of lookahead, zero tokens of lookahead — the one-token peek needed for the `IDENT` statement ambiguity is the parser's job; (4) lexical errors are `T_ERROR` tokens, never fatal, so one pass reports all of them.
- Added `src/main.c` with `--dump-tokens`, and a `Makefile` (`-Wall -Wextra -Wpedantic -std=c11`, sanitizers in the dev target). The Makefile probes for the sanitizer runtime and warns instead of failing when it's absent — `libasan`/`libubsan` aren't installed on this machine (`sudo dnf install libasan libubsan` to enable them).
- Added `tests/lex_all_tokens.pseudo` (every token kind, mixed keyword casing, identifiers that start like keywords, `3.14` vs `arr.length`) and `tests/lex_errors.pseudo` (unterminated string, unexpected character, bare `!`). Ran both and verified the output by hand: all token kinds, lines, and columns correct; all three error kinds reported in a single pass with exit code 65.
- Not yet verified: leak-freedom under Valgrind — not installed on this machine. The only allocation in the program is the source buffer in `read_file`, freed in `main` after the last token is consumed.

## 2026-07-23 — Pre-Phase-1 ambiguity re-check

- Caught a real gap while re-verifying the grammar is LL(1) before starting the parser: `assignment` and `expr_stmt` (a bare call-as-statement) both start with `IDENT`, unlike every other `statement` alternative which leads with a unique keyword. Documented in `docs/grammar.md`'s ambiguity section — `parse_statement()` needs to peek one token past the `IDENT` (`=`/`[` → assignment, `(` → call) rather than dispatching on the first token alone.
- Re-indexed the repo in the codebase knowledge graph (docs-only project, 99 nodes).

## 2026-07-23 — Ambiguity docs (prompted by professor's questions)

- Added "Known ambiguities & how they're resolved" to `docs/grammar.md`: reserved-word collision, dangling-else, operator precedence/associativity, and parser lookahead (LL(1)) — the standard compiler-theory ambiguities, and why this grammar doesn't hit them.
- Added "The keyword-vs-identifier ambiguity (and how this layer resolves it)" to `README.md`'s ML layer section: the adaptive layer is the one place the reserved-word question becomes genuinely ambiguous again (broken input like `fr = 5` could be a typo'd variable or a mangled `for` loop), and the resolution order — grammar validity first, personal history as tiebreaker, honest non-blocking suggestion on a real tie, never a silent guess.

## 2026-07-23 — Phase 0 complete

- Rewrote `docs/grammar.md`: it previously only had the six statement forms with no formal grammar, no lexical token list, and no operator precedence table — even though `README.md`/the design guide already claimed AND/OR/NOT precedence was "covered." Added: full lexical token table, EBNF-style formal grammar (one rule per parser function to come), precedence table, comments syntax, I/O statements, semantic rules, and a "design choices" section explaining *why* each concrete syntax choice was made (case-insensitive keywords, `=` vs `==`, no semicolons, single-line comments only).
- Added `docs/AST_SPEC.md` — AST node shapes for every grammar rule, plus memory-ownership notes, written before any parser code exists.
- Added `docs/IR_SPEC.md` — shared IR instruction set sketch, an AST→IR lowering table, and explicitly flagged open questions (temp-slot allocation, type tagging, string representation) to resolve at the start of Phase 2 rather than silently deciding now.
- Edited `docs/COMPILER_DESIGN_GUIDE.md`: it was generated independently and numbered its phases 1–4, which didn't match `PROJECT_PLAN.md`'s 0–6 numbering — renumbered to match, added a Phase 0 section, and added a note that AI-assisted code must still be commented for review (per the "AI reviews, doesn't generate" philosophy already in the file).
- Created repo scaffolding: empty `src/`, `include/`, `tests/` directories, ready for Phase 1.
- Marked Phase 0 complete in `PROJECT_PLAN.md`.

## 2026-07-23 — Project plan & workflow

- Added `PROJECT_PLAN.md`: phase-by-phase workflow (Phase 0 design → Phase 1 frontend → Phase 2 shared IR → Phase 3 bytecode VM → Phase 4 AOT-to-C → Phase 5 error handling/tooling → Phase 6 deferred ML layer), with exit criteria per phase and a per-phase working workflow.
- Added this file (`PROJECT_LOG.md`) to track changes going forward.
- Re-indexed the repo in the codebase knowledge graph after the doc cleanup below.

## 2026-07-23 — README rewrite

- Rewrote `README.md` from scratch, merging content that used to be spread across the now-deleted `pseudo-compiler-README.md` draft and `pseudocode+ml` proposal dump: dual-backend rationale, full compiler pipeline (added semantic analysis, which the old README was missing), phase table, error-handling spec, ML layer (Phase 6) design, tech stack table, repository layout, working philosophy, and course context (VIT Vellore, original timeline).
- Fixed a stale "License: TBD" line — pointed it at the real `LICENSE` file (MIT) instead.

## 2026-07-23 — Docs cleanup

- Deleted `pseudo-compiler-README.md` — outdated duplicate draft of `README.md`.
- Deleted `pseudocode+ml` — broken file containing stray tool artifacts (`<task_progress>`, `</write_to_file>` tags) wrapping a plaintext copy of content already in `Pseudo_Is_All_You_Need_Project_Proposal.docx`.
- Deleted `.~lock.README.md#` and `docs/.~lock.grammar.md#` — LibreOffice lock files, not project content.
- Renamed `compiler design guide for lesser ui usage ` (bad filename, trailing space) → `docs/COMPILER_DESIGN_GUIDE.md`.

## Earlier history (from git log, pre-cleanup)

- `e12f57b` — Added formal grammar spec: control flow, functions, arrays, I/O, comments (`docs/grammar.md`).
- `9658768` — Fixed a syntax error in the grammar doc.
- `3a660c9` — Deleted a duplicate file.
- `66b1a50` — Removed AI-assisted tooling artifacts, kept the grammar spec.
- `5d41cc0` — Prep notes for using local models on this project.
- Initial commit + `.gitignore` setup.
