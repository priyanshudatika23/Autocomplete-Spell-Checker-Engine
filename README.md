# Autocomplete & Spell-Checker Engine (C++)

A command-line engine that gives **real-time word completions** and **spelling
corrections** from a large English vocabulary, built from scratch with a Trie
and dynamic programming.

## Features

- **Trie (prefix tree)** with fixed 26-pointer nodes for O(1) character transitions.
- **Autocomplete** via DFS over the subtree under a typed prefix.
- **Spell-check** using **Levenshtein edit distance** (dynamic programming),
  space-optimized from O(N×M) to O(M).
- **Trie-guided DP with early pruning** — computes one DP row per node during
  traversal and abandons branches that exceed the edit-distance threshold.
- **Combined ranking** that scores completions against corrections by
  `frequency / penalty^(edit distance)`, so a very common correction (e.g.
  `the`) can outrank a rare literal match (e.g. `teh`), while normal typing
  still favors completions.
- **Per-query timing** with `<chrono>`.

## Build

Requires a C++17 compiler (g++ / clang).

```bash
g++ -std=c++17 -O2 -o engine main.cpp
# or:
make            # builds ./engine and ./benchmark
```

## Run

```bash
./engine            # loads dataset.txt by default
./engine words.txt  # or pass any file in the same format
```

Then type a prefix (e.g. `app`) to autocomplete, or a misspelling (e.g. `teh`)
to correct. Type `:q` to quit.

## Dataset

The included `dataset.txt` is Peter Norvig's word-frequency list — the 1/3
million (333,333) most frequent words derived from the Google Web Trillion Word
Corpus (<https://norvig.com/ngrams/count_1w.txt>, MIT License). Each line is
`word<TAB>frequency`, e.g.

```
the     23135851162
apple   90000
```

You can swap in any file that follows the same `word<TAB or space>frequency`
format.

## Benchmarks

Measured on the full 333,333-word corpus with `benchmark.cpp`
(`make bench`). Absolute numbers vary by machine; the relative costs are the
interesting part.

**Build & memory**

| Metric              | Value       |
|---------------------|-------------|
| Words loaded        | 333,333     |
| Trie nodes          | 805,917     |
| Bytes per node      | 224         |
| Est. Trie footprint | ~172 MB     |
| Nodes per word      | 2.42        |
| Load time           | ~290 ms     |

**Latency** (per query, over 2,000 random queries)

| Path                        | avg      | p95     |
|-----------------------------|----------|---------|
| `autocomplete()` (prefix)   | ~0.12 ms | ~0.45 ms|
| `fuzzySearch()` (2-edit)    | ~3.8 ms  | ~6.0 ms |
| `query()` (both, full call) | ~3.9 ms  | ~5.7 ms |

**Takeaways**

- Prefix completion is effectively instant; fuzzy correction dominates the full
  query cost because it computes a DP row at every visited node.
- Memory is deterministic: the Trie always builds to 805,917 nodes. The fixed
  26-pointer node gives O(1) transitions but is memory-heavy — with ~2.4
  nodes/word and most child slots null, 208 of the 224 bytes per node are
  pointers, the majority unused. A hash-map child table, a DAWG, or a ternary
  search tree would trade some speed for a large memory reduction — the main
  avenue for future work.

## Project structure

```
AutocompleteEngine.hpp   # Trie, Levenshtein DP, ranking, query logic
main.cpp                 # interactive CLI
benchmark.cpp            # load-time / memory / latency harness
dataset.txt              # 333k-word Norvig frequency corpus
Makefile                 # one-command build
```

## How it works (brief)

1. `DataLoader` streams the vocabulary file into the Trie once at startup.
2. `query()` computes prefix completions **and** edit-distance corrections,
   merges them into a single scored list, and returns the top-K.
3. Tuning knob: `PENALTY` in `queryDetailed` controls how strongly each edit is
   penalized — raise it to trust the typed text more, lower it to correct more
   aggressively.

## Acknowledgements

Word-frequency data derived from the Google Web Trillion Word Corpus, compiled
by Peter Norvig (<https://norvig.com/ngrams/>), used under the MIT License.
