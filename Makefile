# Autocomplete & Spell-Checker Engine
# Usage:
#   make          -> build ./engine
#   make run      -> build and run with dataset.txt
#   make clean    -> remove the binary

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
TARGET   := engine
SRC      := main.cpp
HEADERS  := AutocompleteEngine.hpp

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET) dataset.txt

clean:
	rm -f $(TARGET)

.PHONY: all run clean
