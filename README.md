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
make
```

## Run

```bash
./engine dataset.txt        # small sample vocabulary, runs instantly
./engine count_1w.txt       # full 333k-word corpus (see Dataset)
```

Then type a prefix (e.g. `app`) to autocomplete, or a misspelling (e.g. `teh`)
to correct. Type `:q` to quit.

## Dataset

The data file format is one entry per line: `word<TAB or space>frequency`, e.g.

```
the     23135851162
apple   90000
```

A small `dataset.txt` is included so the project runs out of the box. For
realistic results, use Peter Norvig's word-frequency list (the 1/3 million most
frequent words, derived from the Google Web Trillion Word Corpus):
<https://norvig.com/ngrams/count_1w.txt> (MIT License).

## Project structure

```
AutocompleteEngine.hpp   # Trie, Levenshtein DP, ranking, query logic
main.cpp                 # interactive CLI
dataset.txt              # small sample vocabulary
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
