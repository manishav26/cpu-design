"""Check real recursive execution, return addresses, and four-segment output."""
import pathlib
import re
import subprocess
import tempfile

for n in (0, 1, 4, 8):
    source = pathlib.Path('programs/factorial_recursive.asm').read_text()
    source = source.replace('input: .word 4', f'input: .word {n}')
    expected = int(subprocess.check_output(['./build/factorial_recursive', str(n)], text=True))
    with tempfile.TemporaryDirectory() as directory:
        path = pathlib.Path(directory) / 'recursive.asm'
        path.write_text(source)
        normal = subprocess.check_output(['./build/simple16', str(path)], text=True)
        trace = subprocess.check_output(['./build/simple16', '--trace-segments', str(path)], text=True)
    assert trace.endswith(normal)
    assert f'result RAM[0x0041]={expected} ' in normal
    blocks = trace.split('SNAPSHOT ')[1:]
    assert len(blocks) == 9 * n + 10  # initial + 9*n+9 executed instructions
    assert 'INITIAL' in blocks[0] and 'HALT' in blocks[-1]
    calls, returns, stack_pointers = [], [], []
    for block in blocks:
        assert re.findall(r'^SEGMENT (\w+)$', block, re.M) == ['text', 'data', 'heap', 'stack']
        words = re.findall(r'\b([0-9A-F]{4}):([0-9A-F]{4})\b', block)
        assert [int(a, 16) for a, _ in words] == list(range(256))
        ram = {int(a, 16): int(v, 16) for a, v in words}
        assert all(ram[a] == 0 for a in range(0x80, 0xC0))  # no heap allocation
        sp = int(re.search(r'SP=0x([0-9A-F]+)', block)[1], 16)
        stack_pointers.append(sp)
        if ' CALL ' in block:
            calls.append(block)
            assert 'PC=0x0004' in block
            executed = int(re.search(r'executed=0x([0-9A-F]+)', block)[1], 16)
            assert ram[sp] == executed + 1  # CALL saved its continuation
            if len(calls) > 1:
                assert ram[sp + 1] == n - len(calls) + 2  # parent's saved n
        if ' RET' in block:
            returns.append(block)
    assert len(calls) == len(returns) == n + 1
    assert min(stack_pointers) == 0xFF - 2 * n
    assert stack_pointers[-1] == 0x100
print('PASS recursive factorial 0, 1, 4, 8; call frames and all four segments')
