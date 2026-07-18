# Project Log — Pseudo Is All You Need

> Read this file first, every session. Update it after every task.
> Keep entries short: what was done, what was actually verified, next step.
> Never mark something done here unless it was actually run/tested.

---

## Status: Restarted clean — pre-Phase-1

Previous attempt had an untested/unverified false-completion issue
(a task-progress checklist got marked complete without anything being
run or tested, and internal tool syntax leaked into a committed file).
Project reset. Starting over with stricter rules (see .clinerules).

---

## Log

### [Date TBD] — Project reset
- Deleted old scratch files, kept only README + proposal content as reference
- Added PROJECT_LOG.md, PROJECT_CONTEXT.md, .clinerules
- codebase-memory-mcp confirmed working (v0.9.0) — re-index once real code exists
- Next step: Phase 1 — formal grammar design (`docs/grammar.md`), before any code

---

## Next step (always keep this current)

Write `docs/grammar.md` — formal grammar spec covering IF/THEN/ELSE, FOR, WHILE,
REPEAT/UNTIL, FUNCTIONs, arrays. This is the confirmed starting point before any
lexer/parser code is written.
