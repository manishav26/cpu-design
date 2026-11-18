# Integration review after teammate merges

Fetched and verified `origin/main` at `20a399e`, containing CPU fixes `ab19dd0`
and assembler fixes `2c561f2`. Those commits are integrated into the feature
branch through Git history; Member 3 has not rewritten teammate implementations.

| Owner | Original finding | Current status |
|---|---|---|
| Member 2 | MUL signed integer promotion overflow | Fixed: operands widen to uint32_t before multiplying; 65535 squared yields 1 modulo 65536 under UBSan. |
| Member 2 | Indirect address wrap | Fixed: wide effective address checked before narrowing; LOAD/STORE at 65535+65 fault. |
| Member 1 | Branch/CALL target domain | Fixed: assembler rejects targets outside text 0x00..0x3F. |
| Member 1 | Data literal truncation | Fixed: .word accepts -32768..65535 and rejects values outside that range. |
| Member 1 | Missing final newlines | Still present in isa.h, isa.c, assembler.h, assembler.c; strict Clang builds warn. |

The functional fixes above have regression checks in `tests/cli.py`, executed
by `make test` and `make sanitize`. Member 2's `test_fixes.c` is also retained
from main; the Member 3 suite uses assertions and nonzero failure statuses.

README diagram paths and the “16 instructions” wording were corrected as
Member 3 documentation integration: 16 primary groups contain 18 mnemonics.

Combined AddressSanitizer/UBSan stalled on this host in the earlier attempt;
no ASan pass is claimed. UBSan remains the default sanitizer check.
Actual team names, contribution confirmation and final review are still needed.
The December recursive program assignment is outside this contribution.
