// ============================================================================
//  benchmark.cpp - Performance harness for the Autocomplete & Spell-Checker.
//
//  Reports, on whatever corpus you pass:
//    - load time (build the Trie)
//    - memory: Trie node count, bytes/node, estimated Trie size, process RSS
//    - latency (avg / p50 / p95 / max) for three code paths:
//        * autocomplete()  - pure prefix completion
//        * fuzzySearch()   - Levenshtein correction with pruning
//        * query()         - the full user-facing call (runs both)
//
//  Build:  g++ -std=c++17 -O2 -o benchmark benchmark.cpp
//  Run:    ./benchmark dataset.txt
// ============================================================================

#include "AutocompleteEngine.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>

using Clock = std::chrono::high_resolution_clock;
static double msSince(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Resident memory (KB) of this process on Linux; -1 if unavailable.
static long readRSSkb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line.substr(6));
            long kb = -1; iss >> kb; return kb;
        }
    }
    return -1;
}

struct Stats { double avg, p50, p95, mx; };
static Stats summarize(std::vector<double> v) {
    Stats s{0, 0, 0, 0};
    if (v.empty()) return s;
    double sum = 0; for (double x : v) sum += x;
    s.avg = sum / v.size();
    std::sort(v.begin(), v.end());
    s.p50 = v[static_cast<size_t>(0.50 * (v.size() - 1))];
    s.p95 = v[static_cast<size_t>(0.95 * (v.size() - 1))];
    s.mx  = v.back();
    return s;
}
static void printRow(const std::string& name, const Stats& s) {
    std::cout << std::left << std::setw(16) << name << std::right << std::fixed
              << std::setprecision(4)
              << std::setw(12) << s.avg
              << std::setw(12) << s.p50
              << std::setw(12) << s.p95
              << std::setw(12) << s.mx << " ms\n";
}

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "dataset.txt";
    const int SAMPLES = (argc > 2) ? std::stoi(argv[2]) : 2000;

    // ---- Load + time -------------------------------------------------------
    const long rssBefore = readRSSkb();
    AutocompleteEngine engine;

    const auto t0 = Clock::now();
    const int loaded = DataLoader::load(path, engine);
    const auto t1 = Clock::now();

    if (loaded < 0) {
        std::cerr << "Error: could not open '" << path << "'.\n";
        return 1;
    }
    const long rssAfter = readRSSkb();

    // ---- Memory report -----------------------------------------------------
    const std::size_t nodes     = engine.nodeCount();
    const std::size_t words     = engine.wordCount();
    const std::size_t nodeBytes = sizeof(TrieNode);
    const double      trieMB    = engine.approxTrieBytes() / (1024.0 * 1024.0);

    std::cout << "==================== CORPUS ====================\n";
    std::cout << "File               : " << path << "\n";
    std::cout << "Words loaded       : " << loaded << "\n";
    std::cout << "Distinct words     : " << words << "\n";
    std::cout << "Load time          : " << std::fixed << std::setprecision(1)
              << msSince(t0, t1) << " ms\n\n";

    std::cout << "==================== MEMORY ====================\n";
    std::cout << "Trie nodes         : " << nodes << "\n";
    std::cout << "Bytes / node       : " << nodeBytes << " (sizeof TrieNode)\n";
    std::cout << "Est. Trie size     : " << std::setprecision(1) << trieMB << " MB\n";
    if (rssBefore >= 0 && rssAfter >= 0) {
        std::cout << "Process RSS delta  : "
                  << (rssAfter - rssBefore) / 1024.0 << " MB (actual, incl. overhead)\n";
    }
    std::cout << "Nodes / word       : " << std::setprecision(2)
              << (words ? double(nodes) / double(words) : 0) << "\n\n";

    // ---- Build query samples from real words -------------------------------
    // Re-read the file to get a word list we can sample prefixes / typos from.
    std::vector<std::string> vocab;
    vocab.reserve(loaded);
    {
        std::ifstream fin(path);
        std::string line;
        while (std::getline(fin, line)) {
            std::istringstream iss(line);
            std::string w; long long f;
            if (iss >> w >> f) vocab.push_back(normalize(w));
        }
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> pick(0, vocab.size() - 1);

    std::vector<std::string> prefixes, typos, mixed;
    prefixes.reserve(SAMPLES); typos.reserve(SAMPLES); mixed.reserve(SAMPLES);
    for (int i = 0; i < SAMPLES; ++i) {
        const std::string& w = vocab[pick(rng)];
        if (w.empty()) { --i; continue; }

        // prefix: first 3 chars (or whole word if shorter)
        prefixes.push_back(w.substr(0, std::min<size_t>(3, w.size())));

        // typo: substitute one interior character
        std::string t = w;
        if (t.size() >= 2) {
            size_t pos = 1 + (rng() % (t.size() - 1));
            char c = 'a' + (rng() % 26);
            if (c == t[pos]) c = (c == 'z') ? 'a' : c + 1;
            t[pos] = c;
        }
        typos.push_back(t);

        // mixed: alternate prefix / typo to mimic real usage
        mixed.push_back((i % 2) ? typos.back() : prefixes.back());
    }

    // Warm up caches so the first timed call isn't an outlier.
    for (int i = 0; i < 50; ++i) { engine.query(mixed[i]); }

    // ---- Time each path ----------------------------------------------------
    auto timeEach = [](const std::vector<std::string>& inputs, auto fn) {
        std::vector<double> samples;
        samples.reserve(inputs.size());
        for (const auto& in : inputs) {
            const auto a = Clock::now();
            volatile auto r = fn(in);   // volatile: stop the optimizer eliding it
            (void)r;
            const auto b = Clock::now();
            samples.push_back(msSince(a, b));
        }
        return samples;
    };

    auto autoSamples  = timeEach(prefixes, [&](const std::string& s){
                            return engine.autocomplete(s).size(); });
    auto fuzzySamples = timeEach(typos, [&](const std::string& s){
                            return engine.fuzzySearch(s, 2).size(); });
    auto querySamples = timeEach(mixed, [&](const std::string& s){
                            return engine.query(s).size(); });

    std::cout << "============ LATENCY (" << SAMPLES << " queries) ============\n";
    std::cout << std::left << std::setw(16) << "path" << std::right
              << std::setw(12) << "avg" << std::setw(12) << "p50"
              << std::setw(12) << "p95" << std::setw(12) << "max" << "\n";
    printRow("autocomplete()",  summarize(autoSamples));
    printRow("fuzzySearch(,2)", summarize(fuzzySamples));
    printRow("query() [both]",  summarize(querySamples));

    return 0;
}
