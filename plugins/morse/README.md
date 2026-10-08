# morse

Morse code for Wilfred. `morse hello` encodes; `morse .... ..` (dots,
dashes, slashes only) decodes. Enter copies the result.

- A–Z, 0–9, plus `. , ?`. Unknown characters are skipped on encode;
  unknown tokens fail the decode (no result rather than a wrong one).
- Offline, no permissions.

## Build

```bash
cc -shared -fPIC -o morse.so morse.c        # Linux
cc -dynamiclib -o morse.dylib morse.c       # macOS
cl /nologo /LD morse.c /Femorse.dll         # Windows (MSVC prompt)
```

See [dice](../dice/README.md) for install/approval notes (same for every
native flagship).
