# Workflows — Step-by-Step for Each Exception

---

## Documentation Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. HUMAN: Create outline in target .md file                │
│    - Use bullets, not prose                                 │
│    - Include technical details (keys, values, types)       │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. AI: Expand outline to full prose                        │
│    - Follow existing style/tone                             │
│    - Keep technical accuracy                                │
│    - Don't invent features                                  │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. HUMAN: Review EVERY word                                │
│    - Technical accuracy                                     │
│    - Placeholders correct ({key}, {n}, etc.)               │
│    - Cross-references valid                                 │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. HUMAN: Commit with own message                          │
│    git commit -m "docs: add section on X"                  │
└─────────────────────────────────────────────────────────────┘
```

### Script: Regenerate English Locale
```bash
python3 scripts/generate-lang-en.py
```

---

## Minor Minis Workflow

```
┌─────────────────────────────────────────────────────────────┐
│ 1. HUMAN: Add MiniKind enum value                           │
│    enum class MiniKind { ..., MyCmd };                      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. HUMAN: Add parser hook in parse_mini_intent()           │
│    else if (key == "mycmd") {                              │
│        set(MiniKind::MyCmd, rest.empty());                  │
│    }                                                        │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. HUMAN: Add switch case in mini_cards()                  │
│    case MiniKind::MyCmd:                                   │
│        out.push_back(card(...));                           │
│        break;                                              │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. AI: Write help string + simple logic                    │
│    - help.XX entry in locale                               │
│    - card() parameters                                     │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. HUMAN: Verify, compile, test                            │
│    ./build/wilfred mycmd                                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Small Fixes Workflow

```
1. Human: Identify exact line + issue
2. AI: Suggest one-line fix
3. Human: Apply → compile → test
```

### Example
```
Human: "Line 42 in minis.cpp: 'it's' should be 'its'"
AI:     "Fix: change \"it's\" to \"its\" in help.05 string"
Human:  Apply → ./build/wilfred_tests → git commit -m "fix: typo in mini help"
```

---

## Moderate Refactor Workflow

```
1. Human: Define scope (single file, single purpose)
2. AI:    Propose refactored code
3. Human: Line-by-line diff review
4.        Compile + full test suite
```

### Checklist
- [ ] Single file only
- [ ] Single purpose (rename/extract/dead-code)
- [ ] < 30 lines changed
- [ ] No public API changes
- [ ] No data structure changes
- [ ] All tests pass

---

## Test Addition Workflow

```
1. Human: Write test name + expected behavior
   void test_my_feature() {
       // Arrange: human defines inputs
       // Act:    AI writes function call
       // Assert: human defines expected output
   }

2. AI: Write test body (arrange/act/assert)

3. Human: Verify logic compiles + passes
```

### Run Tests
```bash
./build/wilfred_tests
# The in-tree harness has no per-test filter; the whole binary runs quickly.
```

---

## Build/CI Tweaks Workflow

```
1. Human: Describe desired change
   "Add -Wshadow to CMake"
   "Add macOS-14 to CI matrix"

2. AI: Edit CMakeLists.txt or .github/workflows/*.yml

3. Human: Validate
   - cmake --build build
   - act -j test (if act installed)
   - Push → verify CI passes
```

---

## Localization Workflow

```
1. Human: Add key to src/locale/locale.cpp (kEnglish table)
2. Run:   python3 scripts/generate-lang-en.py
3. AI:    Translate lang/en.yml → lang/XX.yml
4. Human: Verify translations (spot-check 10%)
```

### Script
```bash
# Regenerate English reference
python3 scripts/generate-lang-en.py

# Translate (requires API key)
python3 scripts/translate.py --lang de --api-key $DEEPL_KEY
python3 scripts/translate.py --lang fr --api-key $DEEPL_KEY
# ... all 13 languages
```

---

## Universal Pre-Commit Checklist

**Before every commit involving AI assistance:**

- [ ] Human wrote the commit message
- [ ] Human reviewed `git diff` line-by-line
- [ ] Tests pass: `./build/wilfred_tests`
- [ ] No AI in commit message
- [ ] No AI in PR description
- [ ] Human GPG signed: `git commit -S`

---

## Emergency Override Process

If AI *must* be used for forbidden work:

```
1. File GitHub issue: "AI override request: [reason]"
2. Maintainer approves explicitly in issue
3. Commit includes: "AI-assisted: [reason]"
4. Extra thorough review by 2+ humans
```