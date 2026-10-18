# Build: `make`   Clean: `make clean`   Test: `make test`
# onyx has g++ 11.5 (C++20 capable). Override with `make STD=c++17` if needed.
CXX      ?= g++
STD      ?= c++20
CXXFLAGS ?= -O2
CXXFLAGS += -std=$(STD) -Wall -Wextra -ffp-contract=off
LDLIBS   += -lm

SRC := src/main.cpp src/isa.cpp src/memory.cpp src/assembler.cpp src/cpu.cpp
OBJ := $(SRC:src/%.cpp=build/%.o)
BIN := build/isaSim_bin

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDLIBS)

build/%.o: src/%.cpp $(wildcard src/*.h) | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build:
	mkdir -p build

test: $(BIN)
	sh tests/run_tests.sh

clean:
	rm -rf build

.PHONY: all test clean
