#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the guest relay map and hook signatures against the exact official tag.

python tools/check_openrac_010.py --source /path/to/OpenRAC
The source checkout is read only; the tag must already have been fetched.
"""
import argparse
from pathlib import Path
import re
import subprocess

COMMIT = 'd015ea1c6925de90b4be91ddb349af83361d9d4b'
TAG = 'launcher-v0.10.0'
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    args = parser.parse_args()
    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(args.source), *arguments], text=True)
    if git('rev-parse', TAG + '^{}').strip() != COMMIT:
        raise SystemExit('Unexpected release commit; refusing to transfer compatibility evidence.')
    table = git('show', TAG + ':games/rac1/ntsc/config/overlays/us/functions.tsv')
    generated = (ROOT/'skill-points-ra/src/native_map.hpp').read_text()
    for name, symbol in [('kRelay','FUN_L00_00241940'), ('kPointInput','FUN_0021abf8'), ('kPointList','FUN_0021d4a8')]:
        row = next(line.split('\t') for line in table.splitlines() if line.startswith(symbol+'\t'))
        actual = {int(k):int(v,16) for k,v in (part.split(':') for part in row[5].split(','))}
        expected = [int(v,16) for v in re.search(name+r'\[\] = \{([^}]+)',generated).group(1).split(',')]
        if set(actual) != set(range(19)) or expected != [actual[i] for i in range(19)]:
            raise SystemExit('Guest callback map changed: '+name)
    for disc, pad, loader in [('ntsc','FUN_00217328','FUN_L00_00241940'), ('pal','func_002181F0','func_L00_002422D8')]:
        rows = git('show', TAG+f':port/game/rac1-{disc}/libraries.tsv').splitlines()
        for symbol, signature in [(pad,'void (unsigned int, unsigned int, int)'), (loader,'unsigned int (int)')]:
            row = next(line.split('\t') for line in rows if line.startswith(symbol+'\t') and line.split('\t')[3]=='hook')
            if row[-1] != signature:
                raise SystemExit('Hook signature changed: '+symbol)
    print('OpenRAC 0.10.0: all 19 callback maps and PAL/NTSC hook signatures match '+COMMIT)

if __name__ == '__main__':
    main()
