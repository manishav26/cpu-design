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
for name, steps, code, data in [('square', 4, 4, 2), ('factorial', 47, 10, 2),
                                ('add', 5, 5, 3), ('mul', 5, 5, 3)]:
    trace = run('./build/simple16', '--trace', f'programs/{name}.asm')
    blocks = trace.split('SNAPSHOT ')[1:]
    assert len(blocks) == steps + 1
    expected = list(range(code)) + list(range(0x40, 0x40 + data))
    for i, block in enumerate(blocks):
        # every snapshot shows the code and data words as binary | hex | ASCII
        rows = [line.split() for line in block.splitlines() if line.strip().startswith('0x')]
        assert [int(r[0], 16) for r in rows] == expected
        for r in rows:
            assert int(''.join(r[1:5]), 2) == int(r[5], 16) and r[6].startswith('|')
        # every executed instruction shows its three clock ticks
        if i > 0:
            assert all(f' {phase} ' in block for phase in ('FETCH', 'DECODE', 'EXECUTE'))
    assert 'executed=0x' in blocks[-1] and 'HALT' in blocks[-1]
    if name == 'factorial':
        jump = next(b for b in blocks if 'executed=0x0007' in b)
        assert 'JMP' in jump and 'PC=0x0003' in jump
for name in ('add', 'mul'):
    reference = int(run(f'./build/{name}'))
    assert f'result RAM[0x0042]={reference} ' in run('./build/simple16', f'programs/{name}.asm')
assert 'MUL    0111' in run('./build/simple16', '--isa')
for args in [[], ['--bogus', 'programs/square.asm'], ['missing.asm']]:
    assert subprocess.run(['./build/simple16', *args], capture_output=True).returncode != 0
print('PASS native C agreement, RAM snapshots, clock phases, CLI errors')

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