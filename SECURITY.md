# Security Policy

## Supported Versions

Wilfred is under active development. Security fixes are made against the
`main` branch and released in the next tagged version. Only the latest
tagged release is supported with security fixes; there are no long-term
support branches at this time.

| Version | Supported |
|---|---|
| Latest tagged release | ✅ |
| Older releases | ❌ |

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security vulnerabilities.**

Instead, please report it privately using one of these channels, in order of
preference:

1. **GitHub Security Advisories**: open a
   [private security advisory](../../security/advisories/new) on this
   repository. This is the preferred channel — it lets maintainers
   collaborate with you on a fix before any public disclosure.
2. **Email**: if you cannot use GitHub's advisory flow, email
   `security@example.invalid` (replace with the project's real contact
   address) with a description of the issue.

Please include as much of the following as you can:

* A description of the vulnerability and its potential impact.
* Steps to reproduce, or a proof-of-concept.
* The affected version/commit and platform (Windows/macOS/Linux).
* Whether the issue requires local access, a malicious plugin/file, or is
  remotely triggerable (e.g. via the local HTTP API or a crafted index
  source).

You should expect an initial response within **5 business days**. We'll work
with you to confirm the issue, assess severity, and agree on a disclosure
timeline — typically a fix is released before any public write-up. Please
give us a reasonable window to ship a patch before disclosing publicly.

## Scope and Security Considerations

Wilfred is a desktop application that indexes local files, launches
applications, and exposes a few local-only interfaces. The following areas
are the most security-sensitive parts of the codebase and the most valuable
targets for a report:

* **Local HTTP API** (`api.*` in the config, `include/wilfred/ipc/http.hpp`):
  binds to `127.0.0.1` by default and is disabled unless `api.enabled: true`
  and a token is set. If you find a way to reach it from outside localhost,
  bypass the token check, or use it to escape its intended scope (e.g. read
  arbitrary files, execute arbitrary commands), that's a high-severity
  report.
* **Local IPC socket/pipe** (`include/wilfred/ipc/server.hpp`): used by the
  CLI to talk to the running daemon. Any way for another local, unprivileged
  process to hijack or spoof this channel to trigger unintended actions is
  in scope.
* **Native plugin loading** (`include/wilfred/plugin/abi.hpp`,
  `src/plugin/host.cpp`): Wilfred loads `.dll` / `.so` / `.dylib` files from
  the configured plugin directories and calls into them directly with no
  sandboxing. This is intentionally similar to loading any native code —
  the security boundary is "don't put untrusted plugins in your plugin
  directory," not something Wilfred can enforce at runtime. Reports about
  the *loading/discovery* logic (e.g. path traversal that loads a plugin
  from an unexpected location) are in scope; "a malicious plugin can do
  anything" is expected behavior, not a vulnerability.
* **stdio plugins**: spawned as a subprocess per query
  (`run_stdio_once`) with the manifest's `command`/`args`. Issues in how
  arguments or the query string are passed to the child process (e.g.
  injection into a shell rather than passed as argv) are in scope.
* **Sync / backup** (`include/wilfred/sync/backup.hpp`, `sync.*` config):
  pushes/pulls a config+index archive to `sync.url` using `sync.token`.
  Issues with token handling, transport security, or archive parsing
  (`pack_backup_archive` / `unpack_backup_archive`) are in scope.
* **Content indexing** (`search.content_indexing`, `src/search/content.cpp`):
  Wilfred reads file contents for indexing. Issues where indexing a
  specially crafted file leads to memory corruption, excessive resource use,
  or information disclosure are in scope.
* **Query/macro/expression parsing** (`src/query/`, `src/search/macros.cpp`,
  `src/math/expr.cpp`): the calculator explicitly avoids `eval`-style
  execution; if you find a way to achieve arbitrary code execution or
  command execution through a crafted search query, that is high severity.
* **Config/YAML parsing** (`src/config/yaml.cpp`): a hand-rolled YAML
  parser. Memory-safety issues here (buffer overflows, out-of-bounds reads)
  triggered by a malicious config file are in scope.

Out of scope by default:

* Vulnerabilities that require the attacker to already have arbitrary code
  execution on the user's machine (Wilfred's local IPC/API are meant for use
  by the local user, not as a security boundary against local attackers).
* Denial-of-service purely through resource-limit config values a local user
  set themselves.
* Missing security hardening in third-party dependencies bundled for
  convenience but not authored by this project (please report those
  upstream as well).

Thank you for helping keep Wilfred and its users safe.
