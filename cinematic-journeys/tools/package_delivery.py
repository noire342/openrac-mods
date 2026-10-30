#!/usr/bin/env python3
"""Package this standalone mod without traversing native QA data or ISO junctions."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    package = ROOT / 'build/package/cinematic_journeys'
    dll = package / 'lib/windows-x86_64/cinematic_journeys.dll'
    manifest = json.loads((package / 'openrac-mod.json').read_text())
    assert manifest['id'] == 'cinematic-journeys' and manifest['version'] == '0.1.0'
    assert list(manifest['plugin']) == ['windows-x86_64']
    image = dll.read_bytes()
    pe = struct.unpack_from('<I', image, 0x3c)[0]
    assert image[:2] == b'MZ' and image[pe:pe + 4] == b'PE\0\0'
    assert struct.unpack_from('<H', image, pe + 4)[0] == 0x8664
    subprocess.run([sys.executable, str(args.sdk / 'tools/package_mod.py'),
                    str(package), '-o', str(args.output)], check=True)
    native = args.output / 'cinematic-journeys-0.1.0-windows-x86_64.zip'
    source = args.output / 'cinematic-journeys-0.1.0-source.zip'
    paths = [ROOT / name for name in ['CMakeLists.txt', 'README.md', 'LICENSE',
                                      'openrac-mod.json', 'sdk.lock', '.gitignore', '.clang-format']]
    for folder in ['src', 'tests', 'tools', 'docs']:
        paths += sorted(p for p in (ROOT / folder).rglob('*')
                        if p.is_file() and '__pycache__' not in p.parts)
    with zipfile.ZipFile(source, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in paths:
            archive.write(path, 'cinematic-journeys/' + path.relative_to(ROOT).as_posix())
    for path in [native, source]:
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            assert not any('cinematic_qa.dll' in name or 'disc.iso' in name
                           for name in archive.namelist())
    (args.output / 'cinematic-journeys-SHA256SUMS').write_text(
        ''.join(f'{sha(path)}  {path.name}\n' for path in [native, source]), encoding='utf-8')
    (args.output / 'Cinematic-Journeys-LEGGIMI.txt').write_text(
        'Cinematic Journeys 0.1.0 - Windows x64\n\n'
        'Installa cinematic-journeys-0.1.0-windows-x86_64.zip dal menu Mods del launcher.\n'
        'Abilita Cinematic Journeys e avvia nuovamente il gioco.\n\n'
        'SKIP VOLONTARIO: tieni L1 + L2 + R1 + R2 e premi Start.\n'
        'Start da solo non salta piu filmati/cutscene. Menu e pausa restano disponibili.\n'
        'Durata minima del viaggio: 5 secondi di default, regolabile da 0 a 30 nelle\n'
        'impostazioni della mod. Al termine del minimo il gioco completa il passaggio\n'
        'animato in corso: il tempo effettivo puo essere maggiore. Lo skip volontario\n'
        'puo abbreviare il viaggio. Gli arrivi speciali della storia mantengono il finale originale.\n\n'
        'Nessuna patch a OpenRAC, nessun nuovo pannello HUD, nessuna modifica ai salvataggi.\n'
        'Verificato nativamente su NTSC-U; PAL coperto dai test del bridge, non provato in gioco.\n'
        'Non usare --skip-movies per provare la protezione: quel comando di sviluppo\n'
        'salta i filmati prima che la mod possa ricevere input.\n', encoding='utf-8')
    print(json.dumps({'native': str(native), 'nativeBytes': native.stat().st_size,
                      'source': str(source), 'sourceFiles': len(paths),
                      'dllSha256': sha(dll)}, indent=2))


if __name__ == '__main__':
    main()
