# Member 3 validation record

Original source base: `origin/cpu-simulator` (`ae9e0a6`). Feature branch:
`feat/programs-execution-tracing`. Now integrated with fetched `origin/main`
(`20a399e`), which contains both teammate PRs and their functional fixes.

- `make run-example`: square(8) = 64, 4 instructions; factorial(8) = 40320,
  47 instructions. Both halted and stored R0 at labeled result address 0x41.
- `make test`: C engine tests and Python CLI tests pass. Factorial 0, 1, 5, 8
  produces 1, 1, 120, 40320, matching the compiled native C reference.
- UBSan build (`-fsanitize=undefined -fno-sanitize-recover=all`): same tests pass.
- Combined AddressSanitizer/UBSan: processes stalled before test output both
  inside and outside sandbox; terminated. No ASan pass is claimed. `make sanitize`
  defaults to UBSan; `SANITIZERS=address,undefined` enables both explicitly.
- Snapshot totals: square 5; factorial 48. Includes loaded state and HALT.
  Each snapshot includes all 66 words from 0x00 through 0x41.
- Traced final CPU fields and all RAM words equal the existing `cpu_run` result.
  Formatter is checked for state preservation. Step-limit and out-of-text fault
  tests emit expected stderr diagnostics and pass assertions.
- Before integration, probes reproduced four functional defects. After teammate
  fixes, regression tests verify large-operand MUL, invalid indirect LOAD/STORE,
  invalid JMP/CALL targets and oversized/undersized .word literals. All pass
  under UBSan; these functional defects are resolved.
- `make report`: generates a 34-page draft PDF, all listings and snapshots from
  current assembler/simulator output. Tectonic downloaded missing lastpage.sty.
- Strict build still reports missing final newlines in teammate ISA/assembler
  files. They are documented, not silently rewritten.

Names and final team integration review remain required. Generated PDF and
trace artifacts live under ignored `build/`; run `make report` to reproduce.

Rendered PDF review: representative architecture, encoding, listings/results,
initial/intermediate/final snapshot and contribution pages were visually inspected.
Trace text was enlarged to improve readability. Extracted word bounds on all
34 pages stayed within horizontal margins. Numbering runs 01/34 through 34/34.
`git diff --check` passes; teammate CPU/assembler/ISA sources match integrated main.

Post-merge validation: `make sanitize` and `make report` pass against integrated
main, including CPU/assembler fix regressions. Regenerated 34-page PDF results
and integration-status pages were visually inspected; extracted text bounds
on all pages fit horizontal and vertical page margins. No push was performed.

## Teammate review updates

- Added CPU-flow and register-role comments to trace.c, main.c and Part 1
  assembly; Part 1 instruction words and execution counts remain unchanged.
- Moved README build instructions after Part 1, removed the member-specific
  heading and README troubleshooting/draft notes. Validation limitations remain
  recorded here and in KNOWN_ISSUES.md.
- Added separate factorial_recursive.c/.asm, with main calling a recursive
  factorial routine. Inputs 0, 1, 4, 8 return 1, 1, 24, 40320 in 9, 18, 45, 81
  simulated instructions. Each run HALTs and restores SP to 0x100.
- --trace-segments prints all four named 64-word segments at loading and after
  every instruction. Tests inspect all 256 addresses in every snapshot, each
  saved return address and n, CALL/RET counts, unchanged heap and minimum SP.
- For input 4, there are 46 snapshots, five CALLs and five RETs. Deepest SP is
  0xF7. Popped stack words remain in RAM as inactive contents.
- C tests compare all final registers, CPU status fields and RAM against cpu_run
  for recursive cases as well as Part 1; formatting remains read-only.
- make sanitize (UBSan), make report and make recursive-trace pass. Part 1 PDF
  remains 34 pages; validation output was moved together onto one page to avoid
  an isolated continuation. Updated pages were rendered and visually inspected;
  text bounds fit horizontal and vertical margins on every page.
- No CPU/assembler/ISA implementation changes were made. The separate Part 2
  report and final Part 1 team-name/contribution confirmation remain pending.
