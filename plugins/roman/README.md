# roman

Roman numerals for Wilfred. `roman 2026` encodes to `MMXXVI`;
`roman mmxxvi` (numeral letters only) decodes back. Enter copies
the result.

- Range 1–3999 (standard subtractive form: `IV`, `IX`, `XL`, `XC`,
  `CD`, `CM`). Out-of-range or mixed input returns no result rather
  than a wrong one.
- Decode is case-insensitive and ignores spaces.
- Bare `roman` shows the usage form.
- Offline, no permissions.

## Build

```bash
cc -shared -fPIC -o roman.so roman.c          # Linux
cc -dynamiclib -o roman.dylib roman.c         # macOS
cl /nologo /LD roman.c /Feroman.dll           # Windows (MSVC prompt)
```

See [dice](../dice/README.md) for install/approval notes (same for every
native flagship).
