# SIMPLE-16 build.  make = simulator + native C references,  make test = all checks
CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SANITIZERS ?= undefined
CORE = src/isa.c src/assembler.c src/cpu.c src/loader.c src/trace.c
NATIVE = build/factorial build/factorial_recursive build/add build/mul
PROGRAMS = add mul square factorial

.PHONY: all run-example test clean traces sanitize recursive-trace
all: build/simple16 $(NATIVE)
build:
	mkdir -p build

# Simulator
build/simple16: $(CORE) src/main.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -Isrc $(CORE) src/main.c -o $@

# Native C versions of the example programs (for comparing results)
build/%: programs/%.c | build
	$(CC) $(CFLAGS) $< -o $@

build/cli build/recursive: build/%: tests/%.c tests/run.h | build
	$(CC) $(CFLAGS) $< -o $@

build/test: tests/test.c $(CORE) $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/test.c -o $@

run-example: all
	@for p in $(PROGRAMS); do echo "== $$p"; ./build/simple16 programs/$$p.asm; done

test: all build/test build/cli build/recursive
	./build/test
	./build/cli
	./build/recursive

# Full instruction-by-instruction traces into build/traces/
traces: all
	mkdir -p build/traces
	@for p in $(PROGRAMS); do ./build/simple16 --trace programs/$$p.asm > build/traces/$$p.txt; done
	./build/simple16 --isa > build/traces/isa.txt

# Part 2: recursive factorial with all four memory segments
recursive-trace: all
	mkdir -p build/traces
	./build/simple16 --trace-segments programs/factorial_recursive.asm > build/traces/factorial_recursive.txt

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=$(SANITIZERS) -fno-sanitize-recover=all' test

clean:
	rm -rf build