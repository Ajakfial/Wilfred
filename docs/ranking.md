# Ranking

Every candidate record that survives filtering is scored by
`rank_record()` (`include/wilfred/search/rank.hpp`, implemented in
`src/search/rank.cpp`) into a single integer. Results are sorted by score
and truncated to `search.max_results` (`take_top()`). This document explains
what each signal actually measures and how they combine, so that tuning the
weights in `ranking:` (see [configuration.md](configuration.md#ranking--scoring-weights))
is informed rather than trial-and-error.

## Empty query: browsing mode

If the query is empty (`ctx.folded.empty()`), there's no text to match
against, so the score is just:

* `+ ranking.application` if it's an application
* `+ ranking.directory_bonus` if it's a directory
* `+ min(ranking.frequency, historical_selection_count * 20)` from history

This is what powers "browse recent/frequent items" style behavior when the
overlay is opened with no input yet.

## With a query: the gate

For a non-empty query, several independent match signals are computed
first — **name hit** (exact/prefix/substring/fuzzy), **acronym match**,
**path hit**, and **content hit**. If *none* of these matched at all
(`!name_hit && !acro && !path_hit && content_hits <= 0`), the record scores
`0` and is effectively excluded — token/frequency/recency/context signals
below only ever add to a record that already matched the query somehow, they
never make an otherwise-irrelevant record appear.

## Name matching (the biggest signals)

Computed via `score_fuzzy()` (`include/wilfred/search/fuzzy.hpp`) against the
case/diacritic-folded name (`fold_search()`):

1. **Exact match** (`folded == query`) → `+ ranking.exact_name` (1200 by
   default — deliberately the largest single signal, so a query that
   exactly names a file always floats to the top regardless of other
   history).
2. If the name matched at all, exactly one of:
   * **Prefix** (name starts with the query) → `+ ranking.prefix_name`
   * **Substring** (query appears anywhere in the name) →
     `+ ranking.substring_name`
   * Otherwise, a **fuzzy/subsequence** match → `+ ranking.fuzzy_name *
     fuzzy_score / 1000` (scaled by how good the fuzzy match was, out of a
     1000-point internal fuzzy scale)
3. A strong fuzzy match (`fuzzy.score >= 600`) additionally gets
   `+ ranking.word_boundary` (the match aligned to a token/word boundary,
   e.g. `vs code` matching "Visual Studio Code" at the start of a word
   rather than mid-word).

## Acronym matching

`acronym_match()` checks whether the query matches the first letters of
each token in the name (e.g. `vsc` → "**V**isual **S**tudio **C**ode"). If
so, `+ ranking.acronym` — independent of, and in addition to, any name-match
score above.

## Token proximity

The query is tokenized (`ctx.tokens`) and compared against the name's own
tokens (`tokenize_name`). For each query token that has a matching or
prefix-matching name token, that's a "hit." The contribution is:

```
+ (ranking.token_proximity * hits) / token_count
- (ranking.token_proximity * misses) / 2       // penalty for unmatched tokens
```

This rewards multi-word queries where every word appears somewhere in the
name (in any order), and penalizes queries where some words are just
missing entirely — a soft signal, not a hard filter.

## Path component matching

If the query appears anywhere in the full folded path (not just the file
name), `+ ranking.path_component`, halved if the query is very short (≤2
characters, to avoid short substrings matching noise in long paths) and
halved again if neither a name nor acronym match also fired (i.e. a
path-only match is weighted less than a match that's also relevant by
name).

## Content matching

If content indexing found any of the query's tokens in the file's indexed
text (`IndexStore::content_token_hits`), `+ ranking.content_hit * hits /
token_count`, with a small flat bonus if this is the *only* reason the
record matched (no name/acronym hit at all) — so a content-only match still
surfaces, but a filename match is still generally preferred over the same
score from content alone.

## Extension, kind, and directory bonuses

* `+ ranking.extension` if the query text appears in the extension itself
  (helps queries like `cpp` surface `.cpp` files even without an explicit
  `ext:cpp` filter).
* `+ ranking.application` for applications, `+ ranking.directory_bonus` for
  directories — applied unconditionally once a record has passed the match
  gate above, same as in the empty-query case.

## History-based signals

Fed from `HistoryStore` via `RankContext` (`fill_rank_context()`,
`include/wilfred/search/context.hpp`):

* **Frequency** — `+ min(ranking.frequency, times_selected * 20)`: how often
  this exact path has been chosen historically, capped so a handful of
  selections don't dominate over an actual good match.
* **Learned choice** — `+ min(ranking.learned_choice, times_chosen_for_this_query
  * 90)`: specifically, how often *this path* was chosen for *this query
  text* before — a stronger, query-specific version of frequency.
* **Previous selection + recency decay** — if this path was selected at all
  within the last 14 days: a flat `+ ranking.previous_selection`, plus a
  recency-decayed bonus `ranking.recency * 1/(1 + hours_since / 1)` (an
  exponential-ish falloff — very recent selections get close to the full
  `recency` weight, and it decays over the following hours/days).
* **Hour/weekday affinity** (`context_aware` only) — if the previous
  selection's time-of-day is within 2 hours of the current hour,
  `+ ranking.hour_affinity`; if it was the same day of the week,
  an additional `+ ranking.hour_affinity / 2`. This is what lets a query you
  always run "at the start of the day" or "on Mondays" learn to rank the
  usual result higher at those times.

## Context-aware signals (`search.context_aware`)

Also from `RankContext`, populated from recent activity in the current
session/history:

* **Recent parent folder** — if the record's parent directory matches (or
  the record's path is under) a recently-visited folder,
  `+ ranking.context_parent`.
* **Recent extension** — if the record's extension matches one from a
  recent selection, `+ ranking.context_extension`.
* **Recent name** — if the folded name matches (or contains) a recently
  selected name, `+ ranking.context_parent / 2`.
* **Access recency** — if the file's own `atime` is within the last 3 days,
  a smaller separate recency-style bonus using `ranking.access_recency`
  (distinct from the *selection*-recency bonus above — this fires from the
  filesystem's access time even if you never picked it from Wilfred).

## Alias and clipboard signals

* **Alias** (`ranking.alias`) — applied by the query-classification /
  interpreter layer, not `rank_record` itself, when the query matches a
  configured `config.aliases` entry (see
  [configuration.md](configuration.md#aliases--query-shortcuts)).
* **Clipboard overlap** (`ranking.clipboard_overlap`) — applied when the
  query or a candidate overlaps with the current clipboard contents
  (`ClipboardSnapshot`, see [query-language.md](query-language.md#clipboard-as-a-query-source)).

## Extending ranking: `RankPipeline`

Beyond the built-in signals above, `RankPipeline`
(`include/wilfred/search/rank.hpp`) lets additional named scoring functions
be registered at runtime and applied as `extras()` on top of
`rank_record()`'s result, without modifying `rank_record` itself:

```cpp
default_rank_pipeline().add("my_signal",
  [](const RankContext& ctx, const IndexStore& store,
     const IndexRecord& rec, const RankingWeights& w) -> int {
    // return an additional score contribution
  });
```

This is the intended extension point for a fork or an in-process
`SearchProvider` that wants a custom ranking factor without a corresponding
config key — see [plugins.md](plugins.md) for the difference between this
and plugin/provider result sources.

## Tuning weights safely

Because a record must pass the "did it match at all" gate before any of
these signals apply, raising a weight (e.g. `recency`) can't cause
completely unrelated files to start appearing — it can only reorder
*already-matching* results. When tuning, it's usually easier to raise a
weight you want to matter more relative to `exact_name`/`prefix_name` (the
two largest defaults) than to lower everything else.
