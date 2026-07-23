# Pseudo Is All You Need

> A dual-backend pseudocode compiler — write once in plain textbook pseudocode, run it two ways. And eventually, a compiler that learns how *you* specifically type, so it can tell the difference between a real syntax error and just your handwriting.

---

## Overview

Algorithms are usually designed and taught in pseudocode (the DAA/textbook-style notation — `IF/THEN/ELSE`, `FOR`, `WHILE`, `REPEAT/UNTIL`, `FUNCTION`s, arrays), then hand-translated into a real language to actually run. This project removes that translation step: pseudocode is the source language itself, built from scratch in C, and compiled through one shared core into two different execution targets.

Rather than stopping at "parses a toy grammar, interprets it," this project has two real differentiators:

1. **Dual-backend execution** — a single shared intermediate representation (IR) that can be run two ways:
   - **Bytecode VM** — interpreted, fast to start, good for iteration and debugging.
   - **AOT-to-C backend** — compiles all the way down to standalone generated C source, built via `gcc` into a native binary with zero runtime dependency, for real performance.

   Both backends are generated from the **same IR** — nothing is written twice, and both stay behaviorally consistent by construction. This mirrors how production compiler infrastructure like LLVM is structured — one IR, multiple backends. It also opens an edge-deployment angle: the AOT-to-C backend means compiled pseudocode can in principle run on resource-constrained hardware (e.g. microcontrollers), not just a desktop interpreter loop.

2. **Adaptive Input Understanding (deferred, Phase 6)** — an ML layer that watches how a specific user types, learns their personal patterns (recurring typos, shorthand, habitual abbreviations), and combines that with grammar validity at the exact parse position to propose corrections — context-aware rectification, not generic dictionary spell-checking.

---

## Architecture

```
                    ┌────────────────────────┐
   Raw keystrokes → │  Adaptive Input Layer   │ → Cleaned / corrected
                    │   (ML-assisted, Phase 6)│      source text
                    └───────────┬────────────┘
                                │
                                ▼
                        ┌───────────────┐
                        │     Lexer      │
                        └───────┬───────┘
                                ▼
                        ┌───────────────┐
                        │     Parser     │   recursive descent → AST
                        └───────┬───────┘
                                ▼
                        ┌───────────────┐
                        │  Semantic      │   scope resolution, type checking
                        │  Analysis      │
                        └───────┬───────┘
                                ▼
                        ┌───────────────┐
                        │  Shared IR     │   one core, two backends
                        └───┬───────┬───┘
                            │       │
                  ┌─────────┘       └─────────┐
                  ▼                           ▼
          ┌───────────────┐           ┌───────────────┐
          │  Bytecode VM   │           │  AOT → C       │
          │  (interpreter) │           │  (compiled)    │
          └───────────────┘           └───────────────┘
           interpreted,                compiles to
           fast iteration loop         native binary via gcc
```

**Why a shared IR, not two independent pipelines:** the two backends have opposite goals (iteration speed vs. execution speed), but the semantics of the language must be identical either way — an `if` that behaves one way in the VM and another way when compiled would make the whole tool untrustworthy. A single IR is what guarantees both backends agree, and it means every language feature is implemented once, not twice.

---

## Compiler pipeline

- **Lexer** — hand-written tokenizer for the pseudocode grammar.
- **Parser** — recursive descent, one function per grammar rule, producing an AST.
- **Semantic analysis** — scope resolution and type checking over the AST.
- **Codegen** — AST lowered into the common IR, then either compiled to bytecode or emitted as C.
- **Execution** — bytecode VM (stack-based) or AOT compilation to C via `gcc`.

## Project phases

| **Phase** | **Scope** | **Status** |
| :-: | :-: | :-: |
| 0 | Design — this README, architecture, grammar & IR spec | Done |
| 1 | Frontend — lexer, parser, AST, semantic analysis | Not started |
| 2 | Shared IR design + construction from AST | Not started |
| 3 | Bytecode VM backend (stack-based) | Not started |
| 4 | AOT-to-C backend | Not started |
| 5 | Error handling & logging system (see below) + CLI (`run`, `build`, `repl`) | Not started |
| 6 | **ML layer — Adaptive Input Understanding** (see below) | Deferred — starts only after Phase 1–5 are solid |

Phase 6 is deliberately last. It depends on real error-log data produced by Phases 1–5, and building it before the core compiler is stable would mean training against a moving — or synthetic — target.

---

## Error handling & error logs

Every compiler phase (lexer, parser, semantic analysis, codegen) can fail, and a pseudocode compiler's whole value proposition is being *helpful* when it does — this project treats error reporting as a first-class feature, not an afterthought bolted on at the end.

### Error categories

| **Code range** | **Phase** | **Example** |
| :-: | :-: | :-: |
| `E0xx` | Lexical | Unrecognized token, unterminated string |
| `E1xx` | Syntax | Missing keyword, unbalanced block, malformed expression |
| `E2xx` | Semantic | Undefined variable, type mismatch, unreachable code |
| `E3xx` | Codegen | IR lowering failure, backend-specific limitation hit |

### Error log format

Every error is emitted in a single structured line, so it's both human-readable in the terminal and machine-parseable later (this format is also what feeds the ML layer in Phase 6):

```
[E103] [Parser] line 14, col 8: expected 'THEN' after IF condition
[E201] [Semantic] line 22, col 3: variable 'total' used before assignment
[E301] [Codegen] line 40, col 1: recursive call unsupported in AOT-to-C v1
```

### Design principles for error handling

- **Never stop at the first error.** Collect as many real errors as possible in one pass (error recovery / synchronization points in the parser) so the user sees the full picture in one run instead of fixing errors one at a time.
- **Point at the exact token**, not just the line — line + column, always.
- **Every session's errors are logged to a `.errlog` file**, not just printed to the terminal. This is intentional: the log is a durable artifact, and it is the raw training data for Phase 6.
- **Error messages describe what was expected, not just what was found** — e.g. "expected 'THEN' after IF condition" rather than a bare "syntax error".

---

## ML layer — Adaptive Input Understanding (Phase 6, deferred)

### The idea

Most "syntax errors" from a real user aren't random — they're a small, personal, repeating set of habits. Someone might reliably type `fr` instead of `for`, drop closing brackets in a specific pattern, or use a shorthand keyword their own way. Standard error correction (a dictionary spell-checker, or a fixed set of "did you mean" rules) treats all of that as generic noise. This layer treats it as signal — it learns **how a specific person actually types and typos**, and combines that with **grammar validity at the exact parse position** to propose corrections. It's modeling intent, not doing a closest-known-word lookup.

### How it works

1. **Keystroke + commit logging.** While editing, raw keystrokes are logged alongside the code the user eventually commits (saves/compiles). The diff between "what was typed along the way" and "what was finally kept" is the training signal — it captures a user's *own* typo patterns (e.g. consistently typing `wend` for `end`, or transposing two characters in `THEN`).
2. **Personal typo model.** Over time, this builds a small per-user model of likely substitutions — not a generic dictionary, but *this person's* habits. Starting point: edit-distance + n-gram context matching; upgrade path: a small sequence model fine-tuned per user.
3. **Context-aware rectification.** When a token fails to parse, instead of immediately raising a hard `E1xx`, the ML layer proposes candidate corrections, ranked by:
   - edit distance to the failing token,
   - **grammar validity** — does substituting this candidate actually produce a valid parse at this exact position in the AST?,
   - the user's personal typing-history frequency for that substitution.
4. **Logged alongside the original error, not instead of it** — every suggestion is recorded as `[E1xx] ... → suggested correction (confidence: X%)` in the same `.errlog` format, so the correction layer is auditable, not a silent black box.
5. **Feedback loop** — every time a suggestion is accepted or overridden, that outcome feeds back into the model, so it keeps adapting to the user over time rather than staying static.
6. **Never blocking** — the suggestion UI is inline and non-blocking. It never forces a correction, always just offers one.

### Why this depends on Phases 1–5

The training data for this layer *is* the error log format described above. Phase 6 does not start until the core compiler has been in real use long enough to produce a genuine corpus of real errors and real corrections — this is a deliberate sequencing decision, not a scope cut. Building the ML layer first would mean training on synthetic or too-small data.

### Planned components (Phase 6)

- [ ] Keystroke + commit logging pipeline (local, per-user, privacy-respecting by default)
- [ ] Baseline rectifier: edit-distance against valid tokens in current grammar context
- [ ] Personalized model: learns from a given user's log history over time
- [ ] Suggestion UI (inline, non-blocking)
- [ ] Evaluation: does personalization actually reduce real syntax errors over time, measured against the logs

---

## Tech stack

| Component | Choice | Why |
|---|---|---|
| Core compiler (lexer, parser, VM, AOT backend) | C | Manual memory/stack control, performance, matches the AOT backend's own output language |
| Bytecode VM | Custom, stack-based, written in C | Keeps the whole toolchain self-contained; direct reference: *Crafting Interpreters*' `clox` |
| AOT backend | Emits generated C source, shells out to `gcc`/`cc` | Reuses a mature, optimizing backend instead of writing native codegen from scratch |
| Adaptive input layer (Phase 6) | Lightweight per-user model (prototyping in Python; not a large neural net — personalization at this scale doesn't need one) | Fast iteration on the ML side; doesn't need to be as fast as the compiler itself |
| Logging store | SQLite | Simple, local, no server dependency |
| Build | `gcc`, `make` | Compiling the project itself, and the AOT-to-C backend's output |
| Debugging | `valgrind`, `gdb`, `cppcheck` | Memory-safety and correctness verification, mechanically — a second opinion that doesn't hallucinate |
| Version control | Git / GitHub | Incremental, reviewable development history |

---

## Repository layout

```
docs/
  grammar.md                 — formal grammar spec (IF/THEN/ELSE, FOR, WHILE,
                                REPEAT/UNTIL, FUNCTIONs, arrays, I/O, comments)
  COMPILER_DESIGN_GUIDE.md   — phase-by-phase working guide: what to read,
                                what to build, how to test, how to review
README.md                    — this file
LICENSE                      — MIT
Pseudo_Is_All_You_Need_Project_Proposal.docx — original course project proposal
```

---

## Working philosophy

This project is built by hand, not generated. The rule (see [docs/COMPILER_DESIGN_GUIDE.md](docs/COMPILER_DESIGN_GUIDE.md) for the full version): write it yourself first, using AI or any reviewer to review — not to generate. Every phase follows the same loop: read the primary source (*Crafting Interpreters*, the C11 standard, `man malloc`/`realloc`/`free`) → write your own version → get it reviewed → understand *why* any correction is correct before applying it. Nothing gets merged in that isn't understood.

This is a genuinely ambitious scope for a course project — three real subsystems (dual-backend compiler, adaptive ML input layer, and the tooling connecting them). The realistic sequencing is to get the core compiler working end-to-end on both backends *first*, with a boring, no-frills text input, and only add the adaptive input layer once the compiler itself is solid — building the intelligent front-end before the thing it feeds into actually works is a common way these projects stall.

---

## Course context

Submitted as a Compiler Design course project by **Vichruth**, B.Tech Computer Science, VIT Vellore. Originally scoped as a 6–9 week project (grammar + lexer, parser, semantic analysis, bytecode + VM backend, AOT-to-C backend, testing/docs/demo), worked on alongside coursework. The Adaptive Input Understanding layer was added afterward as a Phase 6 extension beyond the original course scope.

---

## Status

Currently in **Phase 0 (design)**. Architecture, grammar, and error-handling design are covered in this README and in `docs/`; Phase 1 (frontend — lexer, parser, AST) is the next concrete implementation step. See [docs/grammar.md](docs/grammar.md) for the grammar spec and [docs/COMPILER_DESIGN_GUIDE.md](docs/COMPILER_DESIGN_GUIDE.md) for the phase-by-phase working guide.

## License

MIT — see [LICENSE](LICENSE).
