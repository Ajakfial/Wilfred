# dice

Dice roller for Wilfred. Type `3d6`, `d20`, `4d6+2` (or `roll …`) and Enter
copies the total; the subtitle shows the breakdown.

- Up to 100 dice, sides 2–1000, optional `+K`/`-K` modifier.
- Bare `dice` / `roll` shows the usage form.
- Offline, no permissions. Rolls use `rand()` seeded per query (tabletop use,
  not cryptography).

## Build

```bash
cc -shared -fPIC -o dice.so dice.c        # Linux
cc -dynamiclib -o dice.dylib dice.c       # macOS
cl /nologo /LD dice.c /Fedice.dll         # Windows (MSVC prompt)
```

Drop the built library next to this `plugin.yml` (or point Wilfred's
`plugins.directories` here after building) and approve it with
`wilfred plugin approve dice`.
