#ifndef AUTOCOMPLETE_ENGINE_HPP
#define AUTOCOMPLETE_ENGINE_HPP

// ============================================================================
//  Autocomplete & Spell-Checker Engine
//  Implements Phases 1-4 of the blueprint:
//    Phase 1 - Data loading / parsing  (DataLoader)
//    Phase 2 - Trie prefix engine      (TrieNode + AutocompleteEngine)
//    Phase 3 - Levenshtein spell-check (standalone DP + Trie-guided DP)
//    Phase 4 - Ranking & integration   (query / queryDetailed)
//  (Phase 5, the interactive CLI, lives in main.cpp)
// ============================================================================

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <cmath>
#include <cstddef>

// ----------------------------------------------------------------------------
//  Small helper: lowercase a token and keep only [a-z] so that the
//  (char - 'a') indexing used by the Trie is always valid.
// ----------------------------------------------------------------------------
inline std::string normalize(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (std::isalpha(ch)) out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

// ============================================================================
//  Step 4 - The Node Struct
//  26 child pointers => O(1) transition between letters.
// ============================================================================
struct TrieNode {
    TrieNode* children[26];
    bool      isEndOfWord;
    long long frequency;   // long long: Norvig counts reach ~23 billion (> int32 max)

    TrieNode() {
        isEndOfWord = false;
        frequency   = 0;
        for (int i = 0; i < 26; ++i) children[i] = nullptr;
    }
};

// ============================================================================
//  Phase 3 (Steps 8 & 9) - Standalone Levenshtein edit distance.
//  These are kept as free functions because the blueprint asks for them
//  explicitly. The engine itself uses the faster Trie-guided version below,
//  but these are handy for testing / reference.
// ============================================================================

// Step 8: classic 2D dynamic-programming table  ->  O(N*M) space.
inline int calculateEditDistance(const std::string& s1, const std::string& s2) {
    const int n = static_cast<int>(s1.size());
    const int m = static_cast<int>(s2.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));

    for (int i = 0; i <= n; ++i) dp[i][0] = i;   // delete all of s1
    for (int j = 0; j <= m; ++j) dp[0][j] = j;   // insert all of s2

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];                 // match, no cost
            } else {
                dp[i][j] = 1 + std::min({ dp[i - 1][j],      // delete
                                          dp[i][j - 1],      // insert
                                          dp[i - 1][j - 1] });// replace
            }
        }
    }
    return dp[n][m];
}

// Step 9: space-optimised version keeping only two rows  ->  O(M) space.
inline int calculateEditDistanceOptimized(const std::string& s1, const std::string& s2) {
    const int n = static_cast<int>(s1.size());
    const int m = static_cast<int>(s2.size());

    std::vector<int> prev(m + 1), curr(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;

    for (int i = 1; i <= n; ++i) {
        curr[0] = i;
        for (int j = 1; j <= m; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                curr[j] = prev[j - 1];
            } else {
                curr[j] = 1 + std::min({ prev[j], curr[j - 1], prev[j - 1] });
            }
        }
        std::swap(prev, curr);
    }
    return prev[m];
}

// ============================================================================
//  The engine: owns the Trie root and exposes insert / search / autocomplete /
//  fuzzy spell-check / ranked query.
// ============================================================================
class AutocompleteEngine {
public:
    AutocompleteEngine()  { root = new TrieNode(); }
    ~AutocompleteEngine() { freeNode(root); }

    // No copying (raw owning pointers) - move only if needed.
    AutocompleteEngine(const AutocompleteEngine&)            = delete;
    AutocompleteEngine& operator=(const AutocompleteEngine&) = delete;

    // ------------------------------------------------------------------
    //  Step 5 - Insert a word with its frequency.
    // ------------------------------------------------------------------
    void insert(const std::string& word, long long freq) {
        const std::string w = normalize(word);
        if (w.empty()) return;

        TrieNode* cur = root;
        for (char c : w) {
            const int idx = c - 'a';
            if (cur->children[idx] == nullptr) {
                cur->children[idx] = new TrieNode();
            }
            cur = cur->children[idx];
        }
        cur->isEndOfWord = true;
        cur->frequency  += freq;   // accumulate: merges counts if two raw
                                   // tokens normalize to the same word
    }

    // ------------------------------------------------------------------
    //  Step 6 - Walk the Trie to the node that ends 'prefix'.
    //           Returns nullptr if the prefix is not present.
    // ------------------------------------------------------------------
    TrieNode* searchPrefix(const std::string& prefix) const {
        TrieNode* cur = root;
        for (char c : prefix) {
            const int idx = c - 'a';
            if (idx < 0 || idx >= 26)        return nullptr;
            if (cur->children[idx] == nullptr) return nullptr;
            cur = cur->children[idx];
        }
        return cur;
    }

    bool hasPrefix(const std::string& prefix) const {
        return searchPrefix(normalize(prefix)) != nullptr;
    }

    // ------------------------------------------------------------------
    //  Step 7 - DFS collector: gather every complete word hanging off
    //           'node', prefixed by 'current'.
    // ------------------------------------------------------------------
    void collectWords(TrieNode* node,
                      std::string current,
                      std::vector<std::pair<std::string, long long>>& out) const {
        if (node == nullptr) return;
        if (node->isEndOfWord) out.emplace_back(current, node->frequency);

        for (int i = 0; i < 26; ++i) {
            if (node->children[i]) {
                collectWords(node->children[i],
                             current + static_cast<char>('a' + i),
                             out);
            }
        }
    }

    // Convenience: all (word, freq) pairs that start with 'prefix'.
    std::vector<std::pair<std::string, long long>>
    autocomplete(const std::string& prefix) const {
        std::vector<std::pair<std::string, long long>> results;
        TrieNode* node = searchPrefix(prefix);
        if (node) collectWords(node, prefix, results);
        return results;
    }

    // ------------------------------------------------------------------
    //  Steps 10 & 11 - Trie-guided Levenshtein with early pruning.
    //  We compute the DP "rows" as we descend the Trie, sharing the
    //  previous row across every branch. If the smallest value in the
    //  current row already exceeds maxCost, the whole subtree is pruned.
    //  Returns tuples of (word, editDistance, frequency).
    // ------------------------------------------------------------------
    std::vector<std::tuple<std::string, int, long long>>
    fuzzySearch(const std::string& target, int maxCost) const {
        std::vector<std::tuple<std::string, int, long long>> results;
        const int cols = static_cast<int>(target.size()) + 1;

        // Row 0 of the DP table: distance from "" to each prefix of target.
        std::vector<int> firstRow(cols);
        for (int i = 0; i < cols; ++i) firstRow[i] = i;

        for (int i = 0; i < 26; ++i) {
            if (root->children[i]) {
                const char letter = static_cast<char>('a' + i);
                fuzzyRecursive(root->children[i], letter, target,
                               firstRow, results, maxCost,
                               std::string(1, letter));
            }
        }
        return results;
    }

    // ==================================================================
    //  Phase 4 - Ranking and integration.
    // ==================================================================
    struct QueryResult {
        bool                     corrected;     // true if spell-check kicked in
        std::vector<std::string> suggestions;   // top-K words
    };

    // ------------------------------------------------------------------
    //  Step 12 - Master search (enhanced beyond the blueprint).
    //
    //  The blueprint runs the spell-checker ONLY when prefix search is
    //  empty. That fails for typos that are themselves valid prefixes
    //  (e.g. "teh" is a real low-frequency web token AND the prefix of
    //  "tehran"), so correction never fires and "the" is never offered.
    //
    //  Instead we ALWAYS compute both, then rank them with one combined
    //  score so a very common correction can beat a rare literal match,
    //  while ordinary typing still favors completions:
    //
    //      completions  -> edit distance 0 (user is mid-word)  -> score = freq
    //      corrections  -> edit distance d (1..threshold)       -> score = freq / PENALTY^d
    //
    //  PENALTY controls how strongly an edit is punished. Larger PENALTY
    //  => more conservative about "correcting" what you typed.
    // ------------------------------------------------------------------
    QueryResult queryDetailed(const std::string& rawInput, int topK = 5) const {
        QueryResult res{ false, {} };
        const std::string input = normalize(rawInput);
        if (input.empty()) return res;

        const int    threshold = 2;        // max edits considered (Step 11)
        const double PENALTY   = 30.0;     // score divisor per edit

        std::unordered_map<std::string, double> bestScore;  // word -> score
        std::unordered_map<std::string, int>    bestDist;   // word -> edit distance

        auto consider = [&](const std::string& w, double score, int dist) {
            auto it = bestScore.find(w);
            if (it == bestScore.end() || score > it->second) {
                bestScore[w] = score;
                bestDist[w]  = dist;
            }
        };

        // Completions: prefix matches, treated as edit distance 0.
        for (const auto& m : autocomplete(input)) {
            consider(m.first, static_cast<double>(m.second), 0);
        }

        // Corrections: words within `threshold` edits, score down-weighted
        // by edit distance. (dist 0 == the input itself, already a completion.)
        for (const auto& c : fuzzySearch(input, threshold)) {
            const std::string& w    = std::get<0>(c);
            const int          dist = std::get<1>(c);
            const long long    freq = std::get<2>(c);
            if (dist == 0) continue;
            const double score = static_cast<double>(freq) / std::pow(PENALTY, dist);
            consider(w, score, dist);
        }

        if (bestScore.empty()) return res;

        // Step 13: sort by the combined score, descending.
        std::vector<std::pair<std::string, double>> cands(bestScore.begin(),
                                                          bestScore.end());
        std::sort(cands.begin(), cands.end(),
                  [](const std::pair<std::string,double>& a,
                     const std::pair<std::string,double>& b) {
                      return a.second > b.second;
                  });

        // Step 14: keep the top K.
        for (int i = 0; i < static_cast<int>(cands.size()) && i < topK; ++i)
            res.suggestions.push_back(cands[i].first);

        // We "corrected" if the best suggestion is an edit, not a completion.
        res.corrected = (bestDist[res.suggestions.front()] > 0);
        return res;
    }

    // Blueprint-named entry point (Step 12 signature).
    std::vector<std::string> query(const std::string& rawInput, int topK = 5) const {
        return queryDetailed(rawInput, topK).suggestions;
    }

    // ------------------------------------------------------------------
    //  Introspection (for benchmarking / analysis).
    // ------------------------------------------------------------------
    std::size_t nodeCount()      const { return countNodes(root); }
    std::size_t wordCount()      const { return countWords(root); }
    std::size_t approxTrieBytes()const { return nodeCount() * sizeof(TrieNode); }

private:
    TrieNode* root;

    static std::size_t countNodes(TrieNode* n) {
        if (n == nullptr) return 0;
        std::size_t c = 1;
        for (int i = 0; i < 26; ++i) c += countNodes(n->children[i]);
        return c;
    }

    static std::size_t countWords(TrieNode* n) {
        if (n == nullptr) return 0;
        std::size_t c = n->isEndOfWord ? 1 : 0;
        for (int i = 0; i < 26; ++i) c += countWords(n->children[i]);
        return c;
    }

    // Recursive helper for fuzzySearch (Steps 10 & 11).
    void fuzzyRecursive(TrieNode* node,
                        char letter,
                        const std::string& target,
                        const std::vector<int>& prevRow,
                        std::vector<std::tuple<std::string, int, long long>>& results,
                        int maxCost,
                        std::string current) const {
        const int cols = static_cast<int>(target.size()) + 1;
        std::vector<int> currentRow(cols);

        // First column = cost of deleting everything matched so far.
        currentRow[0] = prevRow[0] + 1;

        for (int col = 1; col < cols; ++col) {
            const int insertCost  = currentRow[col - 1] + 1;
            const int deleteCost  = prevRow[col]        + 1;
            const int replaceCost = prevRow[col - 1] +
                                    (target[col - 1] == letter ? 0 : 1);
            currentRow[col] = std::min({ insertCost, deleteCost, replaceCost });
        }

        // A complete word within threshold => record it.
        if (currentRow[cols - 1] <= maxCost && node->isEndOfWord) {
            results.emplace_back(current, currentRow[cols - 1], node->frequency);
        }

        // Step 11: prune. If even the best cell already exceeds the
        // threshold, no descendant can do better, so stop here.
        const int rowMin = *std::min_element(currentRow.begin(), currentRow.end());
        if (rowMin <= maxCost) {
            for (int i = 0; i < 26; ++i) {
                if (node->children[i]) {
                    fuzzyRecursive(node->children[i],
                                   static_cast<char>('a' + i),
                                   target, currentRow, results, maxCost,
                                   current + static_cast<char>('a' + i));
                }
            }
        }
    }

    // Recursively free the Trie.
    static void freeNode(TrieNode* node) {
        if (node == nullptr) return;
        for (int i = 0; i < 26; ++i) freeNode(node->children[i]);
        delete node;
    }
};

// ============================================================================
//  Phase 1 (Steps 1-2) - The File Parser.
//  Reads a "word frequency" file line by line into the engine.
//  Returns the number of words loaded, or -1 if the file could not be opened.
// ============================================================================
class DataLoader {
public:
    static int load(const std::string& path, AutocompleteEngine& engine) {
        std::ifstream fin(path);
        if (!fin.is_open()) return -1;

        std::string line;
        int count = 0;
        while (std::getline(fin, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string   word;
            long long     freq = 0;
            if (iss >> word >> freq) {        // e.g. "the   23135851162" (tab or space)
                engine.insert(word, freq);
                ++count;
            }
        }
        return count;
    }
};

#endif // AUTOCOMPLETE_ENGINE_HPP
