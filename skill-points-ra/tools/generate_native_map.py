"""Generate the ABI-compatible callback's guest addresses from OpenRAC's source map."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root.parent/'OpenRAC/games/rac1/ntsc/config/overlays/us/functions.tsv'
text='// Generated from OpenRAC 2a457b30426ebf01caf40b2831f27ae647d7ad4a, NTSC-U.\n#pragma once\n#include <cstdint>\nnamespace skillpoints {\n'
for name,symbol in [('kRelay','FUN_L00_00241940'),('kPointInput','FUN_0021abf8'),('kPointList','FUN_0021d4a8')]:
    row=next(l.split('\t') for l in source.read_text().splitlines() if l.startswith(symbol+'\t'))
    places={int(k):int(v,16) for k,v in (p.split(':') for p in row[5].split(','))}
    assert set(places)==set(range(19))
    text+='inline constexpr std::uint32_t '+name+'[] = {\n    '+', '.join(f'0x{places[i]:08X}' for i in range(19))+'\n};\n'
text+='}\n'
(root/'src/native_map.hpp').write_text(text)
