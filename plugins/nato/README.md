# nato

NATO phonetic alphabet for Wilfred. `nato hello` encodes to
`Hotel Echo Lima Lima Oscar`; `nato hotel echo ...` (known words only)
decodes back. Enter copies the result.

- A–Z via the official words (`Alfa`, `Juliett`, `X-ray`, `Zulu`),
  0–9 via `Zero`–`Niner`. Word breaks encode as `/`.
- Decode accepts common variants (`alpha`, `xray`, `nine`).
- Bare `nato` shows the usage form.
- Offline, no permissions.

## Build

```bash
cc -shared -fPIC -o nato.so nato.c          # Linux
cc -dynamiclib -o nato.dylib nato.c         # macOS
cl /nologo /LD nato.c /Fenato.dll           # Windows (MSVC prompt)
```

See [dice](../dice/README.md) for install/approval notes (same for every
native flagship).
