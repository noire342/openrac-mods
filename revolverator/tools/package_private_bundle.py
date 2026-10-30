#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 noire342
"""Build a portable Windows NTSC-U preview ZIP; never install, launch or publish it.

The arsenal-compat plugin and complete mod sources travel with the bundle.
The installed client, base level assets/manifests and player saves are read on
the user's PC, never included in this archive. A receipt is optional; its absence means
native_gameplay_verified=false, including for preparation-only tests. This outer
bundle is started with its included script, not imported into the stock launcher.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import zipfile

ENGINE_BASE = '77a2f05257c5dfc2ab9b1ec52a84fc78e8f64225'
ENGINE_REPOSITORY = 'https://github.com/OpenRAC/OpenRAC'
SUPPORTED_RUNTIME_SHA256 = ('b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081',)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def member_path(value):
    value = str(value).replace('\\', '/')
    rel = PurePosixPath(value)
    if rel.is_absolute() or PureWindowsPath(value).drive or '..' in rel.parts:
        raise ValueError(f'Unsafe bundle path: {value}')
    return rel.as_posix()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--models', type=Path, required=True)
    parser.add_argument('--ammunition-overlay', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, help='Optional installed stock NTSC-U client: verify its SHA256 read-only, never include it')
    parser.add_argument('--compat-plugin', type=Path, required=True, help='Built arsenal-compat 0.1.0 native package')
    parser.add_argument('--compat-source', type=Path, help='Corresponding GPL provider source; defaults to work/arsenal-compat')
    parser.add_argument('--bridge-patch', type=Path, help='Optional historical engine patch, included only as reference documentation')
    parser.add_argument('--engine-source', type=Path, help='Optional engine checkout for the reference patch license')
    parser.add_argument('--weapon-xp', type=Path, help='Optional compatible Weapon XP API v4 native package')
    parser.add_argument('--weapon-xp-source', type=Path, help='Corresponding GPL XP source; defaults to the isolated arsenal companion')
    parser.add_argument('--hd-icon-data', type=Path, help='Exact generated header used by an HD-enabled binary; included as corresponding artwork data')
    parser.add_argument('--source-commit', default='', help='Published source revision matching this preview')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--validation', type=Path, help='Native receipt for this exact DLL/runtime; never inferred from a build')
    args = parser.parse_args()
    output = args.out.resolve()
    if output.exists():
        raise ValueError('Refusing to overwrite an existing preview bundle')
    source = Path(__file__).resolve().parents[1]
    plugin = args.plugin.resolve(strict=True)
    models = args.models.resolve(strict=True)
    ammo = args.ammunition_overlay.resolve(strict=True)
    entries, inputs, names = {}, {}, set()

    def put(relative, data):
        relative = member_path(relative)
        if PurePosixPath(relative).suffix.casefold() == '.exe':
            raise ValueError(f'Client executables are excluded from this bundle: {relative}')
        if relative.casefold() in names:
            raise ValueError(f'Duplicate bundle path: {relative}')
        names.add(relative.casefold())
        entries[relative] = data

    def take(path, relative):
        path = path.resolve(strict=True)
        if not path.is_file():
            raise ValueError(f'Not a file: {path}')
        data = path.read_bytes()
        inputs[path] = sha(data)
        put(relative, data)

    def read_json(path):
        path = path.resolve(strict=True)
        data = path.read_bytes()
        inputs[path] = sha(data)
        return json.loads(data.decode('utf-8-sig'))

    def native_package(root, expected_id):
        manifest = read_json(root/'openrac-mod.json')
        if manifest.get('format') != 2 or manifest.get('id') != expected_id:
            raise ValueError(f'Expected a built native {expected_id} package')
        if 'ntsc' not in manifest.get('games', {}).get('rac1', []):
            raise ValueError(f'{expected_id} does not support NTSC-U')
        libraries = manifest.get('plugin', {})
        if 'windows-x86_64' not in libraries:
            raise ValueError(f'{expected_id} has no Windows x86-64 library')
        for relative in libraries.values():
            candidate = (root/member_path(relative)).resolve(strict=True)
            if not candidate.is_relative_to(root):
                raise ValueError('A plugin library escapes its package')
        for path in sorted(root.rglob('*')):
            if path.is_file():
                take(path, 'plugin/'+expected_id+'/'+path.relative_to(root).as_posix())
        return manifest

    def full_mod_source(root, identifier):
        ignored = {'.git', '__pycache__', 'build', '.profile', '.sessions'}
        for path in sorted(root.rglob('*')):
            rel = path.relative_to(root)
            if path.is_file() and not ignored.intersection(rel.parts) and path.suffix != '.pyc':
                take(path, 'source/'+identifier+'/'+rel.as_posix())

    runtime_digest = SUPPORTED_RUNTIME_SHA256[0]
    if args.runtime:
        runtime = args.runtime.resolve(strict=True)
        if not runtime.is_file():
            raise ValueError('The optional runtime input must be an existing client file')
        runtime_digest = sha(runtime.read_bytes())
        inputs[runtime] = runtime_digest
        if runtime_digest not in SUPPORTED_RUNTIME_SHA256:
            raise ValueError(f'Unsupported stock NTSC-U client SHA256: {runtime_digest}')

    compat = args.compat_plugin.resolve(strict=True)
    compat_manifest = native_package(compat, 'arsenal-compat')
    if compat_manifest.get('version') != '0.1.0':
        raise ValueError('Expected arsenal-compat 0.1.0')
    compat_source = (args.compat_source or source.parent.parent/'arsenal-compat').resolve(strict=True)
    compat_source_manifest = read_json(compat_source/'openrac-mod.json')
    if (compat_source_manifest.get('id') != 'arsenal-compat'
            or compat_source_manifest.get('version') != compat_manifest.get('version')
            or compat_source_manifest.get('license') != 'GPL-3.0-or-later'):
        raise ValueError('The bundled compatibility source must match the binary version and its GPL license')
    full_mod_source(compat_source, 'arsenal-compat')

    manifest = native_package(plugin, 'revolverator')
    if manifest.get('version') != '0.2.0':
        raise ValueError('This packager requires the additive Revolverator 0.2.0 package')
    starting = [setting for setting in manifest.get('settings', []) if setting.get('key') == 'start_unlocked']
    if len(starting) != 1 or starting[0].get('type') != 'bool' or starting[0].get('default') is not False:
        raise ValueError('The preview requires start_unlocked with its normal default set to false')
    take(source/'LICENSE', 'LICENSE')
    full_mod_source(source, 'revolverator')
    if args.hd_icon_data:
        hd_data = args.hd_icon_data.resolve(strict=True)
        if hd_data.name != 'revolverator_hd_icon_data.hpp':
            raise ValueError('Expected the exact generated HD icon header used for this binary')
        take(hd_data, 'source/revolverator/generated/revolverator_hd_icon_data.hpp')
        put('source/revolverator/generated/NOTICE.txt', (
            'Corresponding generated artwork input for the included HD-enabled binary.\n'
            'Derived from the original Revolverator glyph, upscaled with Real-ESRGAN.\n'
            'Original game artwork remains the property of its respective rights holders.\n'
            'The software GPL license does not relicense this artwork.\n'
        ).encode())
    put('source/revolverator/BUILD-CONFIG.txt', (
        'Build with the separately obtained OpenRAC Mod SDK 1.3.0 and CMake Release x64.\n'
        + ('Set -DREVOLVERATOR_HD_ICON_DIR=<this source directory>/generated.\n'
           if args.hd_icon_data else 'This bundle uses the native glyph fallback; no HD input was supplied.\n')
        + 'See CMakeLists.txt and docs/BUILD_AND_ASSETS.md for the source build commands.\n'
    ).encode())
    xp_manifest = None
    if args.weapon_xp:
        xp_manifest = native_package(args.weapon_xp.resolve(strict=True), 'weapon-xp')
        if xp_manifest.get('version') != '0.6.3-arsenal':
            raise ValueError('The XP provider must be the compatible 0.6.3-arsenal API v4 build')
        xp_source = (args.weapon_xp_source or source.parent.parent/'weapon-xp-arsenal-bridge').resolve(strict=True)
        xp_source_manifest = read_json(xp_source/'openrac-mod.json')
        if (xp_source_manifest.get('id') != 'weapon-xp'
                or xp_source_manifest.get('version') != xp_manifest.get('version')
                or xp_source_manifest.get('license') != 'GPL-3.0-or-later'):
            raise ValueError('The bundled XP source must match the binary version and its GPL license')
        full_mod_source(xp_source, 'weapon-xp')

    patch_digest = None
    if args.bridge_patch:
        patch = args.bridge_patch.resolve(strict=True)
        patch_digest = sha(patch.read_bytes())
        engine_source = (args.engine_source or source.parent.parent/'OpenRAC-arsenal-bridge').resolve(strict=True)
        take(patch, 'source/engine-reference/openrac-arsenal-bridge.patch')
        take(engine_source/'COPYING', 'source/engine-reference/COPYING')
        put('source/engine-reference/REFERENCE.txt', (
            f'Historical engine source repository: {ENGINE_REPOSITORY}\n'
            f'Historical patch base commit: {ENGINE_BASE}\n'
            f'Base archive: {ENGINE_REPOSITORY}/archive/{ENGINE_BASE}.zip\n'
            'This optional GPL patch is reference documentation only.\n'
            'The preview does not include, build, patch or require a replacement client EXE.\n'
            'Use the verified installed stock NTSC-U client with the arsenal-compat plugin.\n'
            'Complete corresponding plugin sources are included under source/arsenal-compat,\n'
            'source/revolverator and, when selected, source/weapon-xp.\n'
        ).encode())

    pack, rotor = read_json(models/'openrac-mod.json'), read_json(models/'rotor.json')
    if pack.get('format') != 1 or pack.get('game') != 'rac1' or pack.get('version') != 'ntsc':
        raise ValueError('Expected the native NTSC-U graphics pack')
    if rotor.get('target_class') != 2017 or rotor.get('native_source_class') != 180:
        raise ValueError('Expected additive2017 using native180; preserve original Walloper art')
    take(models/'openrac-mod.json', 'graphics/openrac-mod.json')
    take(models/'rotor.json', 'graphics/rotor.json')
    for level in range(19):
        name = f'level_{level:02}'
        if pack.get('replaces', {}).get(name, {}).get('mobys') != [2017]:
            raise ValueError(f'Graphics pack must target only additive2017 in {name}')
        for relative in ('mobys/moby_2017.glb', 'textures/moby_revolverator_original.png'):
            take(models/name/relative, f'graphics/{name}/{relative}')
    receipt, recipe = read_json(ammo/'build-receipt.json'), read_json(ammo/'recipe.json')
    if recipe.get('model_kind') != 'industrial_energy_cartridge' or 'bit_centres' in recipe:
        raise ValueError('Expected the single industrial energy cartridge recipe')
    if (receipt.get('visual_class') != 32000 or receipt.get('vendor_visual_class') != 2016
            or receipt.get('weapon_visual_class') != 2017 or len(receipt.get('levels_added', [])) != 19):
        raise ValueError('Expected additive2017/32000/2016 on nineteen gameplay levels')
    scales = {float(info['scale']) for info in receipt['levels_added']}
    if len(scales) != 1:
        raise ValueError('The prebuilt ammunition model requires one native225 scale across levels')
    ammo_model = ammo/'levels/level_01/revolverator_ammo/model.glb'
    take(ammo_model, 'ammunition/model.glb')
    take(ammo_model.parent/'texture.png', 'ammunition/texture.png')
    asset = {'format': 1, 'game': 'rac1', 'disc': 'ntsc', 'native_pickup_class': 225,
             'world_class': 32000, 'vendor_class': 2016, 'weapon_class': 2017,
             'native_weapon_class': 180, 'native_scale': next(iter(scales)),
             'kind': 'industrial_energy_cartridge'}
    put('ammunition/asset.json', (json.dumps(asset, indent=2)+'\n').encode())
    take(source/'tools/Start-Revolverator.ps1', 'Start-Revolverator.ps1')
    for name in ['README.md']+['docs/'+p.name for p in sorted((source/'docs').glob('*.md'))]:
        if (source/name).is_file():
            take(source/name, name)
    verified = False
    if args.validation:
        validation = read_json(args.validation)
        binary = plugin/manifest['plugin']['windows-x86_64']
        if validation.get('windows_dll_sha256', '').lower() != sha(binary.read_bytes()):
            raise ValueError('Native validation belongs to another DLL')
        if validation.get('runtime_sha256', '').lower() != runtime_digest:
            raise ValueError('Native validation belongs to another runtime')
        compat_binary = compat/compat_manifest['plugin']['windows-x86_64']
        if validation.get('compat_dll_sha256', '').lower() != sha(compat_binary.read_bytes()):
            raise ValueError('Native validation does not match the included arsenal-compat DLL')
        verified = validation.get('native_gameplay_verified') is True
        take(args.validation, 'native-validation.json')
    put('Avvia-Revolverator.cmd', (
        '@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Revolverator.ps1" %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    put('Avvia-Prova.cmd', (
        '@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Revolverator.ps1" -Trial %*\r\n'
        'if errorlevel 1 pause\r\n').encode('ascii'))
    put('README-FIRST.txt', (
        'REVOLVERATOR 0.2.0 - ANTEPRIMA WINDOWS NTSC-U\n\n'
        'REQUISITI: Windows a 64 bit, PowerShell 5.1 o successivo e una propria installazione\n'
        'OpenRAC con Ratchet & Clank 1 NTSC-U già estratto. PAL non è supportata da questa anteprima.\n'
        'Non servono Python o un compilatore. Lo ZIP non include client EXE, ISO o salvataggi del giocatore.\n'
        'Si usa il tuo client OpenRAC installato: il file EXE non viene modificato né sostituito.\n'
        'Prima della preparazione si controlla che sia una build NTSC-U verificata.\n'
        'Estrai TUTTO in una cartella scrivibile prima di avviare uno dei due file .cmd.\n\n'
        'PROVA IMMEDIATA: fai doppio clic su Avvia-Prova.cmd. Si parte da Novalis con il\n'
        'Revolverator e 20 cariche, senza caricare o copiare le tue card e i tuoi progressi XP.\n'
        'Le pressioni automatiche superano solo l’avvio del livello; poi i comandi sono tutti tuoi.\n'
        'La prova usa --no-card: non salva. Ogni avvio crea una nuova cartella .sessions.\n\n'
        'PARTITA CON PROGRESSI: Avvia-Revolverator.cmd usa invece una COPIA persistente\n'
        'dei tuoi salvataggi. Normalmente l’arma si compra da Hoven per 15.000 bolts.\n'
        'Per assegnarla subito anche in questa modalità: Avvia-Revolverator.cmd -StartUnlocked.\n'
        'L’opzione non rifornisce un’arma già posseduta e non cancella i progressi se rimossa.\n'
        'Revolverator e Walloper sono distinti: 20 munizioni massime, raccolta di 2, prezzo di 25 per unità.\n'
        'XP, se incluso, si carica prima del Revolverator; -WithoutWeaponXP lo disabilita.\n\n'
        'PERCORSO LOCALE: normalmente si leggono i dati in %USERPROFILE%\\OpenRAC\\active\\rac1\\data.\n'
        'Se li hai altrove, avvia per esempio:\n'
        'Avvia-Prova.cmd -InstalledData "D:\\OpenRAC\\active\\rac1\\data"\n'
        'Il client è cercato in %LOCALAPPDATA%\\OpenRAC Launcher\\openrac\\port\\build\\release.\n'
        'Se è installato altrove, aggiungi -Runtime "D:\\OpenRAC\\openrac-rac1-ntsc.exe".\n'
        'Per preparare senza avviare il gioco: Avvia-Prova.cmd -PrepareOnly.\n\n'
        'SALVATAGGI PERSISTENTI: alla prima preparazione si copia la card SCUS_971.99 in .profile/cards,\n'
        'insieme a impostazioni e profili XP iniziali. Ogni nuova sessione riusa queste COPIE e\n'
        '.profile/mod-data. I progressi del bundle NON vengono riversati sulla card originale.\n'
        'Non cancellare .profile e mantienila quando aggiorni il bundle. Una .profile esistente non\n'
        'viene reinizializzata. I salvataggi in gioco modificano normalmente la copia persistente.\n\n'
        'I manifest sono clonati localmente: si aggiungono 2017/32000/2016, conservando la classe 180.\n'
        'I dati dei livelli originali sono letti tramite link locali, mai inclusi nello ZIP.\n'
        'Il plugin arsenal-compat incluso si carica prima di XP e Revolverator nel client già installato.\n'
        'L’installazione ufficiale e i file originali restano invariati.\n'
        'Ogni file incluso viene verificato tramite SHA256 prima della preparazione.\n'
        'Codice delle nostre mod: GPL-3.0-or-later, Copyright (c) 2026 noire342; vedere LICENSE.\n'
        'source contiene i sorgenti completi di arsenal-compat e delle mod incluse.\n'
        'L’eventuale patch engine in source/engine-reference è solo documentazione: non serve per avviare.\n'
        'Le licenze software non coprono gli asset originali del gioco.\n'
        'Il modello della munizione è stato creato con assistenza AI.\n\n'
        'È una build di anteprima, non una release definitiva o una verifica dell’intera campagna.\n'
        'Questo ZIP si avvia con i .cmd inclusi: NON importarlo come mod nel launcher standard.\n'
        'I limiti della verifica nativa sono nel receipt; build e PrepareOnly non sono prove in gioco.\n'
    ).encode('utf-8'))
    delivery = {'format': 1, 'kind': 'revolverator-preview-bundle', 'game': 'rac1', 'disc': 'ntsc',
                'version': manifest['version'], 'publication': 'PUBLIC PREVIEW',
                'source_commit': args.source_commit,
                'hd_icon_input_included': args.hd_icon_data is not None,
                'native_gameplay_verified': verified, 'full_campaign_verified': False,
                'runtime': 'installed-openrac-rac1-ntsc', 'runtime_included': False,
                'runtime_sha256': runtime_digest, 'supported_runtime_sha256': list(SUPPORTED_RUNTIME_SHA256),
                'runtime_input_verified': args.runtime is not None,
                'compat_included': True, 'compat_version': compat_manifest['version'],
                'compat_dll_sha256': sha((compat/compat_manifest['plugin']['windows-x86_64']).read_bytes()),
                'engine_base_commit': ENGINE_BASE if patch_digest else None, 'bridge_patch_sha256': patch_digest,
                'software_licenses': {'authored_mods': 'GPL-3.0-or-later', 'arsenal_compat': 'GPL-3.0-or-later'},
                'weapon_xp_included': xp_manifest is not None,
                'weapon_xp_version': xp_manifest['version'] if xp_manifest else None,
                'files': {name: {'bytes': len(data), 'sha256': sha(data)} for name, data in sorted(entries.items())}}
    put('delivery.json', (json.dumps(delivery, indent=2)+'\n').encode())
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    if any(sha(path.read_bytes()) != digest for path, digest in inputs.items()):
        output.unlink()
        raise RuntimeError('Input changed during packaging; unstable preview bundle discarded')
    print(json.dumps({'publication': 'PUBLIC PREVIEW', 'native_gameplay_verified': verified,
                      'archive': str(output), 'files': len(entries),
                      'runtime_included': False, 'compat_version': compat_manifest['version'],
                      'weapon_xp_included': xp_manifest is not None,
                      'sha256': sha(output.read_bytes())}, indent=2))


if __name__ == '__main__':
    main()
