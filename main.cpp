// ============================================================================
//  Phase 5 - The Interactive CLI  (Steps 15 & 16)
//  Loads the dataset into the Trie once, then loops: read input, run query(),
//  print the top-5 suggestions and the execution time in milliseconds.
//
//  Build:  g++ -std=c++17 -O2 -o engine main.cpp
//  Run:    ./engine            (uses dataset.txt in the current directory)
//          ./engine words.txt  (use a custom dataset)
// ============================================================================

#include "AutocompleteEngine.hpp"

#include <iostream>
#include <chrono>
#include <string>

int main(int argc, char** argv) {
    // Step 3: fast I/O. Untie/desync the C++ streams from C stdio.
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    const std::string datasetPath = (argc > 1) ? argv[1] : "dataset.txt";

    // Step 15: load the dataset into the Trie exactly once.
    AutocompleteEngine engine;
    const int loaded = DataLoader::load(datasetPath, engine);
    if (loaded < 0) {
        std::cerr << "Error: could not open dataset file '" << datasetPath << "'.\n"
                  << "Pass a path as the first argument, e.g.  ./engine words.txt\n";
        return 1;
    }

    std::cout << "============================================\n"
              << "  Autocomplete & Spell-Checker Engine\n"
              << "============================================\n"
              << "Loaded " << loaded << " words from \"" << datasetPath << "\".\n"
              << "Type a prefix to autocomplete, or a misspelled word to correct.\n"
              << "Commands: ':q' to quit.\n\n";

    // Step 15: the interactive loop.
    std::string input;
    while (true) {
        std::cout << "Start typing: " << std::flush;

        if (!std::getline(std::cin, input)) break;       // EOF (Ctrl-D)
        if (input == ":q" || input == ":quit") break;
        if (input.empty()) continue;

        // Step 16: time the query with <chrono>.
        const auto start = std::chrono::high_resolution_clock::now();
        const auto result = engine.queryDetailed(input, 5);
        const auto end   = std::chrono::high_resolution_clock::now();
        const double ms  =
            std::chrono::duration<double, std::milli>(end - start).count();

        if (result.suggestions.empty()) {
            std::cout << "  (no suggestions within 2 edits)\n";
        } else {
            if (result.corrected)
                std::cout << "  No exact prefix found - did you mean:\n";
            for (std::size_t i = 0; i < result.suggestions.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << result.suggestions[i] << '\n';
            }
        }
        std::cout << "  [" << ms << " ms]\n\n";
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}
