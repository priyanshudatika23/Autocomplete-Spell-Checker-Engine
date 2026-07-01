# Autocomplete & Spell-Checker Engine
# Usage:
#   make          -> build ./engine and ./benchmark
#   make run      -> build and run the interactive CLI on dataset.txt
#   make bench    -> build and run the performance benchmark on dataset.txt
#   make clean    -> remove the binaries

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
HEADERS  := AutocompleteEngine.hpp

all: engine benchmark

engine: main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o engine main.cpp

benchmark: benchmark.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o benchmark benchmark.cpp

run: engine
	./engine dataset.txt

bench: benchmark
	./benchmark dataset.txt

clean:
	rm -f engine benchmark

.PHONY: all run bench clean
