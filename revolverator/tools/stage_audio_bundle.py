#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 lorenzogarrone
"""Stage the frozen original sound handoff; never alter or resample its WAVs."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from authored_audio import digest, safe_read, pcm_parameters, snapshot, EXCLUDED_BY_USER, PREVIEW_ONLY

DESCRIPTIONS = {
 'equip': 'The housing locks into place, then the drill screws into position.',
 'idle_loop': 'A quiet motor while the equipped weapon has ammunition.',
 'attack': 'A quick drill acceleration when Circle is pressed.',
 'capture': 'Metal contact and the motor taking the weight of a captured enemy.',
 'spin_loop': 'A repeating motor layer while an enemy spins.',
 'spin_fast_loop': 'An alternative faster motor layer for listening comparisons.',
 'acceleration_demo': 'A non-looping demonstration of continuous acceleration; audition only.',
 'throw': 'A weighted release and fast launch.',
 'heavy_hit': 'A solid melee impact against a target that cannot be captured.',
 'projectile_impact': 'The impact of a thrown enemy.',
 'gold_explosion_low': 'The lower-charge endpoint of the original Golden explosion design.',
 'gold_explosion_full': 'The full-charge endpoint of the original Golden explosion design.',
 'dry_fire': 'A short motor attempt that fails when ammunition is empty.',
 'ammo_pickup': 'A short original ammunition pickup cue.',
 'gold_acquisition': 'The original Golden upgrade acquisition cue.',
 'xp_level_up': 'The original weapon level-up cue.',
 'early_release': 'The motor winds down as an undercharged enemy is dropped.',
 'ground_contact': 'A light landing, used only when actual ground contact is observed.',
 'charge_ready': 'A short one-shot cue when the charge bar becomes full.'}

README = '''# Original Revolverator audio

These WAVs are original deterministic procedural synthesis, with no game audio
or external sound samples. Their waveform bytes are preserved from the frozen
author handoff. The distribution license is GPL-3.0-or-later.

The English manifest lists each cue, its original SHA256, PCM parameters and
runtime purpose. source/manifest-original.json preserves the original handoff;
provenance.json inventories every distributed file. Both original generators
are included under source/. Generating new audio is a development operation
using NumPy and FFmpeg; players need neither dependency.

The original handoff contains 19 cues. This bundle distributes 14: the custom
ammo_pickup, dry_fire, equip and xp_level_up WAVs are excluded by the user's
request. Native ammunition-pickup and empty-fire sounds remain game-owned;
no custom construction or level-up cue is added. The original source
manifest and generators remain intact. acceleration_demo is an audition-only
demonstration and is not distributed in the runtime bundle. All 14 distributed
WAVs have runtime mappings.
Alternative spin layers are not simultaneous full-volume defaults.

User listening approval and final in-game mix approval are not implied by file
validation. Native validation must separately cover cue timing, pause, equipment
changes, empty ammunition, teardown and the actual gameplay mix.
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--handoff', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--refresh-managed', action='store_true',
                        help='Refresh only an intact previously staged bundle from the same handoff')
    args = parser.parse_args()
    handoff_blob = args.handoff.resolve(strict=True).read_bytes()
    handoff = json.loads(handoff_blob)
    root = Path(handoff['bundle_dir']).resolve(strict=True)
    original_blob = safe_read(root, 'manifest.json')
    if handoff.get('pending') or digest(original_blob) != handoff.get('manifest_sha256'):
        raise ValueError('The audio handoff is incomplete or changed after freezing')
    original = json.loads(original_blob)
    if original.get('pending') or original.get('sources') != ['source/make_sounds.py', 'source/make_extra_cues.py']:
        raise ValueError('The completed original audio source set is required')
    cues = original['cues']
    if {cue['id'] for cue in cues} != set(DESCRIPTIONS) or len(cues) != 19:
        raise ValueError('The completed 19-cue original handoff is required')
    files = {'source/manifest-original.json': original_blob,
             'LICENSE-GPL-3.0.txt': safe_read(root, 'LICENSE-GPL-3.0.txt'),
             'README.md': README.encode()}
    for name in original['sources']:
        files[name] = safe_read(root, name)
    staged = []
    for cue in cues:
        name = cue['id']+'.wav'
        blob = safe_read(root, cue['relative_path'])
        if digest(blob) != cue['sha256'] or any(cue.get(k) != v for k,v in pcm_parameters(blob).items()):
            raise ValueError(f'Original audio file changed: {name}')
        if cue['id'] in EXCLUDED_BY_USER or cue['id'] in PREVIEW_ONLY:
            continue
        files[name] = blob
        item = {key:value for key,value in cue.items() if key not in ('path','description_it')}
        item['relative_path'] = name
        item['description'] = DESCRIPTIONS[cue['id']]
        staged.append(item)
    manifest = {'format':1,'id':'revolverator-original-audio','origin':original['origin'],
        'license':'GPL-3.0-or-later','game_audio_included':False,'listening_approved':False,
        'source_manifest_sha256':digest(original_blob),'sources':original['sources'],
        'source_cue_count':len(cues),'excluded_by_user':list(EXCLUDED_BY_USER),
        'preview_only':list(PREVIEW_ONLY),
        'runtime_notes':original['runtime_notes'],'cues':staged}
    files['manifest.json'] = (json.dumps(manifest,indent=2)+'\n').encode()
    provenance = {'format':1,'source_handoff_sha256':digest(handoff_blob),
        'source_manifest_sha256':digest(original_blob),'waveforms_modified':False,
        'files':{name:{'bytes':len(blob),'sha256':digest(blob)} for name,blob in sorted(files.items())}}
    files['provenance.json'] = (json.dumps(provenance,indent=2)+'\n').encode()
    source = args.source.resolve(strict=True)
    if json.loads((source/'openrac-mod.json').read_text())['id'] != 'revolverator':
        raise ValueError('Expected the Revolverator source tree')
    destination = source/'assets/audio'
    if destination.is_symlink() or not destination.resolve().is_relative_to(source):
        raise ValueError('Audio destination must stay inside the selected source tree')
    if destination.exists():
        if not args.refresh_managed:
            raise ValueError('Audio directory exists; use --refresh-managed only for an intact staged bundle')
        old = json.loads(safe_read(destination.resolve(), 'provenance.json'))
        listed = old.get('files', {})
        present = {p.relative_to(destination).as_posix() for p in destination.rglob('*') if p.is_file()}
        if present != set(listed)|{'provenance.json'} or old.get('source_manifest_sha256') != digest(original_blob):
            raise ValueError('Existing audio is not an intact bundle from the same handoff')
        for name,pin in listed.items():
            blob = safe_read(destination.resolve(), name)
            if pin != {'bytes':len(blob),'sha256':digest(blob)}:
                raise ValueError('Existing managed audio changed; refusing to overwrite it')
        obsolete = set(listed)-set(files)
        unused_override = {'source/equip-060/'+name for name in (
            'manifest-original.json','handoff-original.json','LICENSE-GPL-3.0.txt',
            'source/make_equip_060.py','source/make_sounds.py')}
        if obsolete-set(key+'.wav' for key in (*EXCLUDED_BY_USER,*PREVIEW_ONLY))-unused_override:
            raise ValueError('Refresh would remove files outside the explicit user exclusions')
        for name in obsolete:
            path = destination/name
            if not path.resolve(strict=True).is_relative_to(destination.resolve()):
                raise ValueError('Excluded audio path escaped the managed directory')
            path.unlink()
    else:
        destination.mkdir(parents=True,exist_ok=False)
    for name,blob in files.items():
        path = destination/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
    copied,info = snapshot(source,required=True)
    print(json.dumps({'staged_files':len(copied),'destination':str(destination),**info},indent=2))

if __name__ == '__main__':main()
