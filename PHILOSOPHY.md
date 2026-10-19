# PHILOSOPHY.md -- The Soul of Wilfred

---

## Why Wilfred Exists

Wilfred was born from frustration. Every launcher, every search tool, every "productivity app" made the same compromises:

- **Cloud-first, local-second** -- Your data belongs to you, not a server.
- **Electron bloat** -- 200MB RAM for a search bar? No.
- **Closed ecosystems** -- Lock-in is a feature for vendors, a bug for users.
- **AI slop** -- "Smart" features that are neither smart nor features.

Wilfred rejects all of it.

---

## Core Tenets

### 1. Local-First, Always

Your files, your history, your clips, your notes -- they never leave your machine unless *you* explicitly sync them. No telemetry. No "anonymous usage statistics." No phoning home. The only network calls Wilfred makes are the ones *you* initiate: weather, web search, package installs, AI (with your own keys).

> **If it can work offline, it must work offline.**

### 2. Human-Crafted Code

Every line of Wilfred is written by a human who understands it. No AI-generated algorithms. No Copilot suggestions accepted blindly. No "vibe coding."

> **If you can't explain it, you don't own it. If you don't own it, it doesn't ship.**

This is why `AGENTS.md` exists. AI is a tool -- a compiler, a search engine, a rubber duck. It is not a contributor.

### 3. Performance Is a Feature

Not "fast enough." *Fast.*

- Cold start: <100ms
- Search latency: <50ms p95 on 100K items
- Idle RAM: <30MB base + index
- Idle CPU: <0.5%

No background indexing thrashing your disk. No GC pauses. No JIT warmup. C++20, zero-cost abstractions, cache-friendly data structures.

> **Speed is respect for the user's time.**

### 4. Cross-Platform Without Compromise

Windows. macOS. Linux. FreeBSD. OpenBSD. NetBSD. DragonFly. Android. iOS.

Not "ports." One codebase. Platform-specific backends for the things that *must* be platform-specific (filesystem watchers, hotkeys, overlays, volume/brightness). Everything else shared.

> **A tool that works everywhere is a tool you can trust everywhere.**

### 5. Extensible, Not Bloated

Plugins. Scripts. Minis. Workflows. Quicklinks. Macros.

You extend Wilfred *your way*. The core stays lean. If you want a feature, write a plugin -- or a mini -- or a script. The kernel doesn't grow.

> **A small core that does one thing well beats a large core that does everything poorly.**

### 6. Privacy by Architecture

Not "we promise not to look." *We can't look.*

- No account system
- No cloud sync (unless you configure it yourself)
- No auto-updates phoning home
- Your index is a local file you can delete, backup, inspect

> **The best privacy policy is "there is no data to protect."**

### 7. Honest About Limitations

Every platform has gaps. Every OS has quirks. Wilfred documents them.

- BSD WiFi/Bluetooth? Marked unsupported.
- Linux Wayland layer-shell? Works on Sway/KDE, not GNOME.
- OpenBSD `pledge`/`unveil`? Audited and clean.
- iOS sandbox? Index only, no overlay.

> **A tool that claims to do everything does nothing well.**

---

## What Wilfred Is Not

- **Not a "smart" assistant** -- It doesn't guess. It finds.
- **Not a file manager** -- It opens file managers.
- **Not an IDE** -- It launches editors.
- **Not a package manager** -- It searches them.
- **Not a window manager** -- It talks to yours.

Wilfred is a **search layer**. The universal `Ctrl+Alt+W` (or `⌘+Alt+W`) that sits above everything and finds anything.

---

## The Name

*Wilfred* -- Old English *wil* (will, desire) + *frith* (peace, protection).

**"The will to find, the peace of knowing it's yours."**

---

## For Contributors

You are not "adding features." You are **preserving the philosophy.**

- Does this PR add dependencies? **Reject.**
- Does it phone home? **Reject.**
- Does it slow down the hot path? **Reject.**
- Does it require an account? **Reject.**
- Is it AI-generated? **Reject.**

Write code you're proud to sign your name to. Code that will still compile in 20 years. Code that respects the human on the other side of the screen.

---

*Wilfred is not a product. It's a promise.*