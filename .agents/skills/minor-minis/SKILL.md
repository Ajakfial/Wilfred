---
name: minor-minis
description: Add a small mini command to src/search/minis.cpp from a human-written skeleton. Use only for trivial additions under 50 lines, such as a help string, a simple payload, or one new alias to an existing mini.
---

# Minor Minis

Minis are the inline results shown by the launcher. This is the narrowest code
exception in `AGENTS.md`: AI may fill in help text and simple logic **after a
human has built the skeleton**.

## Allowed

- A new mini command under 50 total changed lines
- A help-string typo fix
- One new alias on an existing mini
- A simple regex/pattern update

## Forbidden

- Adding a new `MiniKind` category (the human does this)
- Any change to `execute_toggle_payload` or `execute_mini`
- Window-management minis or other system integration
- Logic over ~50 lines, or anything that needs new state, threads, IPC, or
  platform calls
- New dependencies

If the request crosses any of these lines, stop and hand it back.

## Preconditions (human does these first)

1. Add the enum value in `include/wilfred/search/minis.hpp`.
2. Add the parser hook in `parse_mini_intent()` in `src/search/minis.cpp`.
3. Add the `switch` case in `mini_cards()`.

Only then do you write the card body and strings.

## What you write

1. **Help text** in the `kEnglish` table in `src/locale/locale.cpp`, e.g.
   `{"help.XX", "mycmd [args]  \u00b7  description"}`.
2. **Locale keys** for the card title/subtitle if the mini adds new strings.
3. **Card parameters** in the existing `case MiniKind::MyCmd:` branch:
   title, subtitle, payload, kind label, score, `ResultAction`, meter.

Use the `.agents/templates/new-mini.cpp` template for the exact shape. Match
the neighboring cases; do not restructure them.

## Workflow

1. Read the three touched regions plus two neighboring minis.
2. Confirm the human's skeleton compiles as-is.
3. Add the smallest possible card/string changes.
4. Build (see `project-orientation`).
5. Ask the human to run `wilfred <command>` and verify behavior manually.
6. Run the relevant tests if the mini has any.

## Guardrails

- Do not add hidden behavior (network calls, file writes, process spawns) to
  make the mini "more useful".
- Do not translate the help string; localization is a separate workflow.
- Do not rename existing keys, payloads, or action IDs.
- Keep the payload stable — external configs and tests may reference it.
- If the diff grows past 50 lines, stop and ask for a human implementation.

## Done when

- The human's skeleton remains intact.
- The card renders the intended strings and copies/performs the intended
  payload.
- A human has verified the command and owns the commit.
