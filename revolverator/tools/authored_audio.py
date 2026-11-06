#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 lorenzogarrone
"""Read only the original, manifest-listed audio and its pinned source evidence."""
from __future__ import annotations
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import wave

DIRECTORY = 'assets/audio'
ID = re.compile(r'^[a-z][a-z0-9_]{0,63}$')
SHA256 = re.compile(r'^[0-9a-f]{64}$')
EXCLUDED_BY_USER = ('ammo_pickup', 'dry_fire', 'equip', 'xp_level_up')
PREVIEW_ONLY = ('acceleration_demo',)

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def required_for_version(version: str) -> bool:
    return tuple(int(part) for part in version.split('-',1)[0].split('.')) >= (0,3,4)

def safe_read(root: Path, name: str) -> bytes:
    relative = PurePosixPath(name)
    if relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name:
        raise ValueError(f'Unsafe authored-audio path: {name}')
    path = root.joinpath(*relative.parts)
    if path.is_symlink() or not path.resolve(strict=True).is_relative_to(root):
        raise ValueError(f'External authored-audio path: {name}')
    return path.read_bytes()

def pcm_parameters(blob: bytes) -> dict:
    with wave.open(io.BytesIO(blob), 'rb') as stream:
        result = {'sample_rate': stream.getframerate(), 'channels': stream.getnchannels(),
                  'bits_per_sample': stream.getsampwidth()*8, 'sample_frames': stream.getnframes()}
        if stream.getcomptype() != 'NONE' or result['channels'] != 1 or result['bits_per_sample'] != 24:
            raise ValueError('Authored audio must be mono 24-bit PCM WAV')
        if result['sample_rate'] != 48000 or not 0 < result['sample_frames'] <= 480000:
            raise ValueError('Authored audio must be 48 kHz and at most ten seconds per cue')
        frames = stream.readframes(result['sample_frames'])
        if len(frames) != result['sample_frames']*3:
            raise ValueError('Truncated authored PCM audio')
    return result

def snapshot(source: Path, required: bool = False) -> tuple[dict[str, bytes], dict]:
    source = source.resolve(strict=True)
    candidate = source / DIRECTORY
    root = candidate.resolve()
    if candidate.is_symlink() or not root.is_relative_to(source):
        raise ValueError('Authored audio must stay inside the mod source tree')
    if not root.exists():
        if required:
            raise ValueError('This release requires its original authored audio bundle')
        return {}, {}
    manifest_blob = safe_read(root, 'manifest.json')
    provenance_blob = safe_read(root, 'provenance.json')
    manifest = json.loads(manifest_blob)
    provenance = json.loads(provenance_blob)
    if manifest.get('format') != 1 or manifest.get('game_audio_included') is not False:
        raise ValueError('Missing original-audio provenance declaration')
    if manifest.get('license') != 'GPL-3.0-or-later' or provenance.get('format') != 1:
        raise ValueError('Unsupported authored-audio license or provenance format')
    original_blob = safe_read(root, 'source/manifest-original.json')
    original = json.loads(original_blob)
    if digest(original_blob) != manifest.get('source_manifest_sha256'):
        raise ValueError('Original audio handoff manifest changed')
    if original.get('pending') or not original.get('origin'):
        raise ValueError('The original audio handoff is incomplete')
    sources = manifest.get('sources')
    if sources != original.get('sources') or not isinstance(sources, list) or not sources:
        raise ValueError('Missing corresponding original audio generators')
    files = {'manifest.json': manifest_blob, 'source/manifest-original.json': original_blob,
             'LICENSE-GPL-3.0.txt': safe_read(root, 'LICENSE-GPL-3.0.txt'),
             'README.md': safe_read(root, 'README.md')}
    for name in sources:
        if not isinstance(name, str) or not name.startswith('source/') or not name.endswith('.py'):
            raise ValueError('Audio generator must be a Python source inside source/')
        files[name] = safe_read(root, name)
    cues = manifest.get('cues')
    originals = original.get('cues')
    if not isinstance(cues, list) or not 1 <= len(cues) <= 128 or not isinstance(originals, list):
        raise ValueError('Invalid original audio cue catalog')
    by_id = {cue.get('id'): cue for cue in originals}
    excluded = list(EXCLUDED_BY_USER)
    if manifest.get('excluded_by_user') != excluded or manifest.get('source_cue_count') != len(originals):
        raise ValueError('Native ammunition and empty-fire sounds must remain excluded')
    if manifest.get('preview_only') != list(PREVIEW_ONLY):
        raise ValueError('The audition demonstration must stay out of the runtime bundle')
    expected_ids = set(by_id)-set(excluded)-set(PREVIEW_ONLY)
    if len(by_id) != len(originals) or len(cues) != len(expected_ids):
        raise ValueError('Audio handoff has duplicate or missing cues')
    if manifest.get('overrides'):
        raise ValueError('No custom cue source overrides are enabled in this release')
    seen = set()
    for cue in cues:
        key = cue.get('id')
        if not isinstance(key, str) or not ID.fullmatch(key) or key in seen or key not in expected_ids:
            raise ValueError('Invalid or repeated authored-audio cue id')
        seen.add(key)
        name = cue.get('relative_path')
        if name != f'{key}.wav':
            raise ValueError('Runtime cue path must match its canonical id')
        wanted = cue.get('sha256', '')
        if not SHA256.fullmatch(wanted) or wanted != by_id[key].get('sha256'):
            raise ValueError('Audio cue does not match the original handoff')
        blob = safe_read(root, name)
        if digest(blob) != wanted:
            raise ValueError(f'Authored audio changed: {key}')
        parameters = pcm_parameters(blob)
        if any(cue.get(field) != value or by_id[key].get(field) != value for field, value in parameters.items()):
            raise ValueError(f'PCM parameters differ from the audio manifest: {key}')
        if cue.get('loop') != by_id[key].get('loop') or cue.get('use') != by_id[key].get('use'):
            raise ValueError(f'Audio loop or purpose changed during staging: {key}')
        files[name] = blob
    if seen != expected_ids:
        raise ValueError('The staged audio cue set differs from the original/user exclusion policy')
    inventory = {name: {'bytes': len(blob), 'sha256': digest(blob)} for name, blob in sorted(files.items())}
    if provenance.get('files') != inventory or provenance.get('source_manifest_sha256') != digest(original_blob):
        raise ValueError('Authored audio provenance inventory does not match the bundle')
    present = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    if present != set(files) | {'provenance.json'}:
        raise ValueError('Unlisted files found in the authored-audio directory')
    files['provenance.json'] = provenance_blob
    return {DIRECTORY+'/'+name: blob for name, blob in files.items()}, {
        'manifest_sha256': digest(manifest_blob), 'provenance_sha256': digest(provenance_blob),
        'source_manifest_sha256': digest(original_blob), 'cue_count': len(cues),
        'original_handoff_cue_count': len(originals), 'excluded_by_user': excluded,
        'preview_only': list(PREVIEW_ONLY),
        'game_audio_included': False, 'listening_approved': False}
