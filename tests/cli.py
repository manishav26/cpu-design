"""Black-box checks: native reference, selected RAM completeness, CLI errors."""
import pathlib
import subprocess
import tempfile


def run(*args):
    return subprocess.check_output(args, text=True)


for n in (0, 1, 5, 8):
    reference = int(run('./build/factorial', str(n)))
    source = pathlib.Path('programs/factorial.asm').read_text().replace('input: .word 8', f'input: .word {n}')
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / 'case.asm'
        path.write_text(source)
        result = run('./build/simple16', str(path))
        assert f'result RAM[0x0041]={reference} ' in result
for name, steps in [('square', 4), ('factorial', 47)]:
    trace = run('./build/simple16', '--trace', f'programs/{name}.asm')
    blocks = trace.split('SNAPSHOT ')[1:]
    assert len(blocks) == steps + 1
    for block in blocks:
        words = [word for line in block.splitlines() for word in line.split()
                 if len(word) == 9 and word[4] == ':']
        assert [int(w[:4], 16) for w in words] == list(range(0x42))
    assert 'executed=0x' in blocks[-1] and 'HALT' in blocks[-1]
    if name == 'factorial':
        jump = next(b for b in blocks if 'executed=0x0007' in b)
        assert 'JMP' in jump and 'PC=0x0003' in jump
for args in [[], ['--bogus', 'programs/square.asm'], ['missing.asm']]:
    assert subprocess.run(['./build/simple16', *args], capture_output=True).returncode != 0
print('PASS native C agreement, complete RAM snapshots, CLI errors')

# Regressions for the teammate fixes now integrated from main.
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory) / 'probe.asm'
    path.write_text('.text\nLOAD R0, [large]\nMUL R0, R0\nHALT\n.data\nlarge: .word 65535\n')
    assert 'R0=1 (0x0001)' in run('./build/simple16', str(path))
    for operation in ('LOAD', 'STORE'):
        path.write_text(f'.text\nLOAD R0, [large]\n{operation} R1, [R0+65]\nHALT\n.data\nlarge: .word 65535\n')
        result = subprocess.run(['./build/simple16', str(path)], capture_output=True, text=True)
        assert result.returncode != 0 and f'{operation} address out of range' in result.stderr
    for source, error in [
        ('.text\nJMP 64\n', 'outside the text segment'),
        ('.text\nCALL 64\n', 'outside the text segment'),
        ('.text\nHALT\n.data\nx: .word 65536\n', 'does not fit in 16 bits'),
        ('.text\nHALT\n.data\nx: .word -32769\n', 'does not fit in 16 bits'),
    ]:
        path.write_text(source)
        result = subprocess.run(['./build/simple16', str(path)], capture_output=True, text=True)
        assert result.returncode != 0 and error in result.stderr
print('PASS merged CPU and assembler fix regressions')
