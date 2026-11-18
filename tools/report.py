"""Generate report evidence only from the real assembler, CPU and tests."""
from pathlib import Path
import subprocess
import shutil

out = Path('build/report')
out.mkdir(parents=True, exist_ok=True)

def run(*args):
    return subprocess.check_output(args, text=True)

shutil.copyfile('report/report.tex', out / 'report.tex')
(out / 'tests.txt').write_text(run('./build/test') + run('python3', 'tests/cli.py'))
listings = []
snapshots = []
rows = []
for name in ('square', 'factorial'):
    program = f'programs/{name}.asm'
    (out / f'{name}-listing.txt').write_text(run('./build/simple16', '--listing', program))
    listings.append(r'\subsection*{' + name.title() + r' assembled listing}' + '\n' +
                    r'\begin{table}[h]\caption{Assembler-produced addresses, words, bit fields and source: ' + name + '.}' + '\n' +
                    r'\VerbatimInput[fontsize=\small]{' + name + r'-listing.txt}\end{table}' + '\n')
    trace = run('./build/simple16', '--trace', program)
    (out / f'{name}-trace.txt').write_text(trace)
    # Each snapshot becomes a captioned figure; two per page, no truncation.
    blocks = trace.split('SNAPSHOT ')[1:]
    for i, block in enumerate(blocks):
        block = block.split('HALT steps=')[0]
        filename = f'{name}-{i:03}.txt'
        (out / filename).write_text('SNAPSHOT ' + block)
        if i % 2 == 0:
            snapshots.append('\\clearpage\n')
        snapshots.append(r'\begin{figure}[h]\centering\begin{minipage}{\linewidth}' + '\n' +
                         r'\VerbatimInput[fontsize=\small]{' + filename + '}\n' +
                         r'\end{minipage}\caption{' + name.title() + f' snapshot {i}: ' +
                         ('loaded RAM' if i == 0 else 'after executed instruction') + r'.}\end{figure}' + '\n')
    normal = run('./build/simple16', program)
    value = normal.split('result RAM[0x0041]=')[1].split()[0]
    expected = '64' if name == 'square' else run('./build/factorial').strip()
    steps = normal.split('steps=')[1].split()[0]
    rows.append(f'{name} & 8 & {expected} & {value} & {steps} & {len(blocks)} \\\\\n')
(out / 'listings.tex').write_text('\n'.join(listings))
(out / 'snapshots.tex').write_text('\n'.join(snapshots))
(out / 'results.tex').write_text(r'\begin{table}[h]\centering\begin{tabular}{lrrrrr}\toprule' + '\n' +
    r'Program & Input & Expected & Actual & Steps & Snapshots\\\midrule' + '\n' + ''.join(rows) +
    r'\bottomrule\end{tabular}\caption{Simulator results; factorial expected value comes from native C.}\end{table}')
print('Generated report evidence in build/report')
