# Project Context — Pseudo Is All You Need

> Static reference. Doesn't change often. For day-to-day state, see PROJECT_LOG.md.

## What this is

A compiler for standard textbook pseudocode (IF/THEN/ELSE, FOR, WHILE,
REPEAT/UNTIL, FUNCTIONs, arrays), written from scratch in C. Course project
for Compiler Design, VIT Vellore.

## Core architecture

Dual-backend design — one frontend (lexer → parser → semantic analysis) feeds
a shared intermediate representation (IR), consumed by two backends:
1. **Bytecode compiler + stack-based VM** — fast to iterate, good for debugging
2. **AOT-to-C codegen** — emits standalone C, compiled via gcc, zero runtime
   dependency, opens the door to microcontroller/edge deployment

Mirrors how real compiler infra (LLVM) separates IR from backend targets.

## Pipeline

Lexer → Parser (recursive descent → AST) → Semantic Analysis
(scope + type checking) → Codegen (AST → common IR → bytecode)
→ Execution (bytecode VM, or AOT compile to C via gcc)

## Tech stack

| Component | Tool |
|---|---|
| Language | C |
| Build | gcc, make |
| Version control | Git / GitHub |
| Debugging | valgrind, gdb |
| Design reference | Crafting Interpreters (Nystrom) |

## Phases (from original proposal)

1. Grammar design + Lexer (~1 week)
2. Parser, recursive descent (1-2 weeks)
3. Semantic Analysis (~1 week)
4. Bytecode + VM backend (2-3 weeks)
5. AOT-to-C backend (~1 week)
6. Testing, docs, demo prep (~1 week)

Total estimate: 6-9 weeks, alongside coursework.

## Explicitly deferred (not this phase)

The "adaptive input understanding" ML layer (learns a user's personal typing
patterns to intelligently correct/complete ambiguous input) is a real planned
feature but is explicitly OUT OF SCOPE until the core compiler works end-to-end
on both backends with plain, unassisted text input. Do not implement or
scaffold this until the core compiler is solid — see original README rationale.

## Local dev environment

- Fedora 44 + Hyprland
- Ollama: qwen2.5-coder:7b (small edits), qwen3-coder:30b (structural work)
- Cline in VS Code, Plan/Act mode split
- MCPs: context7 (live docs), duckduckgo-search, codebase-memory-mcp (code graph)