CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
SANITIZERS ?= undefined
CORE = src/isa.c src/assembler.c src/cpu.c src/loader.c src/trace.c

.PHONY: all run-example test clean traces report sanitize
all: build/simple16 build/factorial
build:
	mkdir -p build
build/simple16: $(CORE) src/main.c $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -Isrc $(CORE) src/main.c -o $@
build/factorial: programs/factorial.c | build
	$(CC) $(CFLAGS) $< -o $@
build/test: tests/test.c $(CORE) $(wildcard src/*.h) | build
	$(CC) $(CFLAGS) -Isrc $(CORE) tests/test.c -o $@
run-example: all
	./build/simple16 programs/square.asm
	./build/simple16 programs/factorial.asm
test: all build/test
	./build/test
	python3 tests/cli.py
traces: all
	mkdir -p build/traces
	./build/simple16 --trace programs/square.asm > build/traces/square.txt
	./build/simple16 --trace programs/factorial.asm > build/traces/factorial.txt
report: test traces
	python3 tools/report.py
	tectonic --outdir build/report build/report/report.tex
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=$(SANITIZERS) -fno-sanitize-recover=all' test
clean:
	rm -rf build
