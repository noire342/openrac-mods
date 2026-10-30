"""Summarize isolated native runs. The fixture requests travel; it does not prove map interaction."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def log(name):
    return (ROOT / ('qa-' + name) / 'data/openrac-rac1-ntsc.log').read_text(encoding='utf-8')


def flight(name, minimum):
    text = log(name).split('QA FIXTURE: requesting native planet 2 -> 3 transition;', 1)[1]
    rows = [(int(ship), int(busy), int(phase), float(clock)) for ship, busy, phase, clock in
            re.findall(r'trace pad=13c940 ship=(\d+) busy=(\d+) phase=(\d+) level=-?\d+ clock=([\d.]+)', text)]
    beginning = next(clock for ship, busy, phase, clock in rows if ship == 4 and busy == 1)
    postponed = []
    for before, after in zip(rows, rows[1:]):
        if before[:3] == (4, 1, 1) and after[:3] == (4, 1, 0):
            elapsed = before[3] - beginning
            assert elapsed < minimum and after[3] - before[3] < .1
            postponed.append(round(elapsed, 3))
    assert postponed, 'native flight never needed the minimum'
    assert any(ship == 4 and phase == 2 and clock - beginning >= minimum
               for ship, busy, phase, clock in rows)
    assert 'now playing level 3' in text and 'frames after control: the run ends' in text
    return {'minimumSeconds': minimum, 'postponedFinalPassAtSeconds': postponed,
            'arrivedOnPlanet3': True, 'fixtureRequestedTravel': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    protected = log('movies-protected')
    deliberate = log('movies-chord-final')
    assert 'movie at sector 40165: played' in protected
    assert 'movie at sector 40165: Start pressed' not in protected
    assert 'movie at sector 40165: Start pressed (pad 0xf0f7)' in deliberate
    assert 'movie at sector 40165: skipped' in deliberate
    assert 'control at frame 2311' in log('protected-start')
    assert 'control at frame 1213' in log('chord-trace')
    dll = ROOT / 'build/package/cinematic_journeys/lib/windows-x86_64/cinematic_journeys.dll'
    for name in ['movies-chord-final', 'flight-default-final']:
        tested = ROOT / ('qa-' + name) / 'plugins/cinematic-journeys/lib/windows-x86_64/cinematic_journeys.dll'
        assert sha(dll) == sha(tested), 'native proof must match the delivered binary'
        status = json.loads((ROOT / ('qa-' + name) / 'plugins/status.json').read_text())
        assert any(m['id'] == 'cinematic-journeys' and m['state'] == 'loaded'
                   for m in status['mods'])
    report = {
        'version': '0.1.0', 'dllSha256': sha(dll),
        'hostTests': {'groups': 2, 'padCombinations': 65536, 'discs': ['pal', 'ntsc']},
        'nativePort': {'version': '0.9.0', 'commit': '2a457b30', 'disc': 'ntsc', 'api': '1.4'},
        'movieStartBlocked': True, 'movieFullChordSkips': True,
        'nativeSceneStartOnlyControlFrame': 2311, 'nativeSceneChordControlFrame': 1213,
        'flights': [flight('flight-default-final', 5), flight('flight-min10', 10)],
        'palNativeVerified': False, 'allScenesVerified': False,
        'originalSaveFilesUsed': False, 'openracPatched': False,
        'notes': ['Flight fixtures use the native transition entry; no manual planet-map proof.',
                  'The final DLL was tested for movie skip and the default 5 second flight.',
                  'The 10 second flight preceded the report-restoration ownership refinement.']
    }
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
