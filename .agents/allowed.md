# Allowed AI Use — Exceptions Only

Per `AGENTS.md`, AI is permitted **only** for these specific tasks. Each has strict workflows.

---

## 1. Documentation (`docs/`, `README.md`, `*.md`)

### What's Allowed
- Expanding human-written outlines/bullets into prose
- Rewriting paragraphs for clarity
- Adding cross-references
- Generating tables from data
- Translating (with human review)

### Workflow
```
1. Human writes outline/bullets in target file
2. AI expands to full prose
4. Human reviews EVERY WORD for accuracy
5. Human commits with own message
```

### Example Prompt
> "Expand these bullets into a section on 'index.roots':
> - List of directories to scan
> - Supports glob patterns
> - Default: ~/Documents, ~/Downloads, /usr/share/applications"

### Forbidden
- Writing docs from scratch without human outline
- Inventing features/options that don't exist
- Writing CLI help text from scratch

---

## 2. Minor Minis (`src/search/minis.cpp`)

### What Counts as "Minor"
- Adding a new mini command (< 50 lines total)
- Fixing typo in help text
- Updating regex pattern
- Adding one new alias to existing mini

### Workflow
```
1. Human writes C++ skeleton (switch case, parser hook, MiniKind enum)
2. AI writes help string + simple logic
3. Human verifies logic, compiles, tests manually
```

### Template: New Mini
```cpp
// In parse_mini_intent():
else if (key == "mycmd") {
    set(MiniKind::MyCmd, rest.empty());
}

// In mini_cards() switch:
case MiniKind::MyCmd:
    out.push_back(card("My Cmd", "Description", "payload", "label"));
```

### Forbidden
- Writing new mini *categories* (new `MiniKind` enum value)
- Complex logic (> 50 lines)
- Anything touching `execute_toggle_payload` or `execute_mini`

---

## 3. Small Fixes

### Allowed
- One-line bug fixes
- Typo corrections in comments/strings
- `#include` fixes
- `const` correctness
- `[[maybe_unused]]` additions

### Workflow
```
1. Human identifies exact line(s)
2. AI suggests fix
3. Human applies, compiles, runs tests
```

### Script: `scripts/fix-typos.py`
```bash
# Find common typos in source
python3 scripts/fix-typos.py --check
# DANGEROUS - human must review diff!
python3 scripts/fix-typos.py --fix
```

---

## 3. Moderate Refactors

### Allowed
- Renaming variables/functions (IDE-style)
- Extracting helper function (< 30 lines)
- Dead code removal
- `if/else` → `switch` or ternary
- Adding `const`, `noexcept`, `[[nodiscard]]`

### Workflow
```
1. Human identifies scope (single file, single purpose)
2. AI suggests refactored version
3. Human reviews diff line-by-line
4. Compile + full test suite
```

### Forbidden
- Cross-file refactors
- Changing data structures
- Changing public APIs
- Moving code between modules

---

## 4. Test Additions

### Allowed
- Adding `TEST_CASE` for existing function
- Edge case coverage for existing mini
- Regression test for fixed bug

### Workflow
```
1. Human writes test name + expected behavior
2. AI writes test body (arrange/act/assert)
4. Human verifies logic, runs test
```

### Template
```cpp
void test_my_feature() {
    using namespace wilfred;
    // Arrange
    Config cfg;
    // Act
    auto result = my_function(cfg, "input");
    // Assert
    CHECK_EQ(result, expected);
}
```

### Script: `scripts/add-test.py`
```bash
# Scaffold a new test
python3 scripts/add-test.py --name test_foo --file tests/test_foo.cpp
```

---

## 5. Build/CI Tweaks

### Allowed
- Adding `-Wwarning` to CMake
- Updating GitHub Actions matrix
- Adding `ctest` timeout
- Fixing flaky test retry logic

### Workflow
```
1. Human describes desired change
2. AI edits `.github/workflows/*.yml` or `CMakeLists.txt`
3. Human validates syntax, runs locally if possible
```

### Forbidden
- Adding new CI jobs/stages
- Changing compiler versions
- Modifying release workflow

---

## 6. Localization (`lang/*.yml`)

### Workflow
```
1. Human adds new key to `src/locale/locale.cpp` (kEnglish table)
2. Run `python3 scripts/generate-lang-en.py`
3. AI translates `lang/en.yml` → `lang/XX.yml`
4. Human verifies translations
```

### Script: `scripts/translate.py`
```bash
# Translate en.yml to target language (requires API key)
python3 scripts/translate.py --lang de --api-key $DEEPL_KEY
python3 scripts/translate.py --lang fr --api-key $DEEPL_KEY
# ... etc for all 13 languages
```

### Forbidden
- Adding new keys without human approval
- Machine-translating without human review