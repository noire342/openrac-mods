#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the ABI-compatible callback map from a pinned OpenRAC revision."""
import argparse
from pathlib import Path
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path, help='OpenRAC git checkout, read only')
    parser.add_argument('--revision', default='launcher-v0.10.0')
    args = parser.parse_args()
    def git(*parameters):
        return subprocess.check_output(['git','-C',str(args.source),*parameters],text=True)
    commit = git('rev-parse',args.revision+'^{}').strip()
    source = git('show',args.revision+':games/rac1/ntsc/config/overlays/us/functions.tsv')
    text = '// Generated from OpenRAC '+commit+', NTSC-U.\n#pragma once\n#include <cstdint>\nnamespace skillpoints {\n'
    for name,symbol in [('kRelay','FUN_L00_00241940'),('kPointInput','FUN_0021abf8'),('kPointList','FUN_0021d4a8')]:
        row = next(line.split('\t') for line in source.splitlines() if line.startswith(symbol+'\t'))
        places = {int(k):int(v,16) for k,v in (part.split(':') for part in row[5].split(','))}
        if set(places) != set(range(19)):
            raise SystemExit('Expected all 19 overlays for '+symbol)
        text += 'inline constexpr std::uint32_t '+name+'[] = {\n    '+', '.join(f'0x{places[i]:08X}' for i in range(19))+'\n};\n'
    text += '}\n'
    (Path(__file__).resolve().parents[1]/'src/native_map.hpp').write_text(text)

if __name__ == '__main__':
    main()
