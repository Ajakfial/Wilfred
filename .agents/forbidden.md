# Forbidden AI Use — Zero Tolerance

**Any violation = immediate permanent ban + public shaming in `SHAME.md`**

---

## 1. Core Algorithms

| Area | Files | Why |
|------|-------|-----|
| Search engine | `src/search/engine.cpp`, `src/search/rank.cpp`, `src/search/fuzzy.cpp` | Core IP |
| Index structures | `src/index/engine.cpp`, `src/index/store.cpp`, `src/index/tokenizer.cpp` | Data integrity |
| Ranking/ranking weights | `src/search/rank.cpp` | Quality depends on human tuning |
| Fuzzy matching | `src/search/fuzzy.cpp` | Damerau-Levenshtein logic |
| Query parser | `src/query/interpreter.cpp`, `src/query/classify.cpp` | Core UX |

---

## 2. Platform Internals

| Area | Files | Why |
|------|-------|-----|
| Filesystem watchers | `src/platform/*/fs_watch.cpp` | OS-specific, subtle bugs |
| Global hotkeys | `src/platform/*/hotkey.cpp` | Security-sensitive |
| Overlay UI | `src/ui/win_overlay.cpp`, `src/ui/mac_overlay.mm`, `src/ui/x11_overlay.cpp` | Platform-specific rendering |
| Volume/brightness | `src/platform/*/launch.cpp` | Hardware control |
| Autostart | `src/platform/*/autostart.cpp` | Persistence mechanism |

---

## 3. System Integration

| Area | Files | Why |
|------|-------|-----|
| Power actions | `src/search/actions.cpp` (lock/sleep/shutdown) | System control |
| Window management | `src/search/minis.cpp` (windows mini) | Window control |
| Media keys | `src/platform/*/launch.cpp` | Hardware media control |
| Window management | `src/search/layouts.cpp` | Window positioning |
| Process control | `src/search/actions.cpp` (kill_process) | Process termination |

---

## 4. Architecture Decisions

- Adding/removing modules
- Changing data structures (IndexRecord, SearchResult, Config)
- New dependencies (CMake, vcpkg, Conan)
- Public API changes (IPC, plugin ABI)
- Module boundaries

---

## 5. Security-Sensitive Code

- Auth/crypto (`src/sync/`, `src/updater/`)
- Sandboxing (`src/platform/ios/`, `src/platform/android/`)
- Privilege escalation (`src/service/autostart.cpp`)
- IPC protocol (`src/ipc/protocol.cpp`)
- Plugin trust (`src/plugin/trust.cpp`)

---

## 6. Performance-Critical Paths

- Hot loops in search/ranking
- Memory allocators (`src/core/mmap.cpp`, `src/core/string_pool.cpp`)
- SIMD/intrinsics (`src/search/fuzzy.cpp`, `src/search/hnsw.cpp`)
- Lock-free code (`src/index/engine.cpp`)
- Hot path allocations

---

## 7. Process Violations

| Violation | Consequence |
|-----------|-------------|
| Committing AI-generated code without human review | Ban |
| AI-authored PR description | Ban |
| AI-authored commit message | Ban |
| Pushing without human GPG signature | Rejected |
| AI-generated PR (description + code) | Ban + shame |
| "I only used AI a little" as defense | Ban + shame |

---

## No Exceptions

> **"I only used AI a little" is not a defense.**
> **"I reviewed it" is not a defense if the code was AI-generated.**
> **"It was just boilerplate" is not a defense.**

The line is simple: **Did a human write the logic?** If no → ban.