# Member 3 validation record

Local source base: `origin/cpu-simulator` (`ae9e0a6`), feature branch
`feat/programs-execution-tracing`. Local refs were used without network fetch;
this does not assert current GitHub PR merge status.

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
- Separate pre-existing issue probes reproduced MUL overflow (UBSan abort),
  indirect address wrap (unexpected success), oversized .word truncation, and
  out-of-text jump execution fault. These are excluded from passing example tests.
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
`git diff --check` passes; teammate CPU/assembler/ISA sources remain unchanged.
