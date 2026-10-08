# password

Password generator for Wilfred. Enter copies; nothing is stored.

| Query            | Result                              |
| ---------------- | ----------------------------------- |
| `password`       | 16 characters (letters+digits+marks) |
| `password 32`    | 4–128 characters                     |
| `password words` | 4 memorable words (`words 6` = 2–12) |
| `password pin`   | 6-digit PIN (`pin 8` = 4–12 digits)  |

Entropy comes from the OS (`rand_s` on Windows, `getentropy` elsewhere).
If OS entropy is unavailable the card says "weak RNG" — treat those
outputs as placeholders, not secrets. Shell-safe alphabet (no quotes or
backslashes).

## Build

Same single-file recipe as [dice](../dice/README.md).
