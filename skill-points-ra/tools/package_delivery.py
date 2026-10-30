"""Create and verify the native and source deliverables. Never packages QA profiles."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from zipfile import ZipFile, ZIP_DEFLATED

root=Path(__file__).resolve().parents[1]
outputs=root.parent.parent/'outputs'
outputs.mkdir(exist_ok=True)
assert '100% tests passed' in (root/'test-final.log').read_text()
native_evidence=(root/'qa-kerwan-off/data/openrac-rac1-ntsc.log').read_text()
enabled_evidence=(root/'qa-kerwan-on/data/openrac-rac1-ntsc.log').read_text()
assert 'mod skill-points-ra 0.1.2 loaded' in native_evidence
assert 'mod skill-points-ra 0.1.2 loaded' in enabled_evidence
assert 'QA achievement selection kerwan=0 active=177 points=657' in native_evidence
assert 'QA achievement selection kerwan=1 active=178 points=667' in enabled_evidence
assert 'Punto stile ottenuto: 253912' in native_evidence
assert 'xp_level_events=0' in native_evidence and 'xp_level_events=1' not in native_evidence
assert 'QA XP observer subscribe=0 token=' in native_evidence
assert 'QA normal controller route reached' in native_evidence and 'no page or selection writes' in native_evidence
pause_evidence=(root/'qa-final-normal-route/data/openrac-rac1-ntsc.log').read_text()
assert 'QA native pause visit=5 seventh_visible=1 story_completed=0' in pause_evidence
assert 'QA: requested existing skill points page through menu.next' not in native_evidence
level_report=root/'native-level-smokes.json'
levels=json.loads(level_report.read_text(encoding='utf-8-sig'))
assert len(levels)==19 and {row['level'] for row in levels}==set(range(19))
assert all(row['loaded'] and row['uiReady'] and not row['errors'] for row in levels)
sdk=root/'.sdk/openrac-mod-sdk-1.3.0'
if not sdk.exists(): sdk=root.parent/'rac1-mods/.sdk/openrac-mod-sdk-1.3.0'
package=root/'build/package/skill_points_ra'
subprocess.run([sys.executable,str(sdk/'tools/package_mod.py'),str(package),'-o',str(outputs)],check=True)
native=outputs/'skill-points-ra-0.1.2-windows-x86_64.zip'
with ZipFile(native,'a',ZIP_DEFLATED,compresslevel=9) as archive:
    for file in (root/'docs').glob('*.md'): archive.write(file,'docs/'+file.name)
    archive.write(root/'include/skill_points_api.h','include/skill_points_api.h')
    archive.write(root/'examples/achievement_observer.c','examples/achievement_observer.c')
with ZipFile(native) as archive:
    assert archive.testzip() is None
    manifest=json.loads(archive.read('openrac-mod.json'))
    assert manifest['id']=='skill-points-ra' and manifest['version']=='0.1.2'
    setting=next(s for s in manifest['settings'] if s['key']=='kerwan_time_trial')
    assert setting['type']=='bool' and setting['default'] is False
    assert manifest['load']['after'][0]=='weapon-xp'
    dll=archive.read(manifest['plugin']['windows-x86_64'])
    pe=struct.unpack_from('<I',dll,0x3c)[0]
    assert dll[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',dll,pe+4)[0]==0x8664
    assert not any(n.endswith(('.spra','.lock','.pdb')) or 'qa-' in n for n in archive.namelist())
source=outputs/'skill-points-ra-0.1.2-source.zip'
folders=['src','include','docs','data','examples','tools','tests',
         'vendor/rcheevos/include','vendor/rcheevos/src/runtime','vendor/rcheevos/src/util']
files=[root/f for f in ['CMakeLists.txt','openrac-mod.json','README.md','LICENSE','LICENSE.rcheevos','.clang-format','.gitignore','sdk.lock','dependencies.lock.json','vendor/rcheevos/LICENSE','vendor/rcheevos/README.md']]
for folder in folders:
    files.extend(p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
with ZipFile(source,'w',ZIP_DEFLATED,compresslevel=9) as archive:
    for file in sorted(set(files)):
        archive.write(file,'skill-points-ra/'+file.relative_to(root).as_posix())
with ZipFile(source) as archive:
    assert archive.testzip() is None
    assert not any(any(part.startswith('qa-') for part in Path(n).parts[1:-1]) or '/research/' in n or '/.git/' in n or n.endswith(('.spra','.lock.tmp','.pdb')) for n in archive.namelist())
    patch=json.loads(archive.read('skill-points-ra/data/3070.json').decode('utf-8-sig'))
    assert len(patch['Achievements'])==178 and sum(a['Points'] for a in patch['Achievements'])==667
    assert set(patch)=={'ID','Title','ConsoleID','Achievements'}
shutil.copyfile(root/'README.md',outputs/'skill-points-ra-LEGGIMI.md')
shot=root/'qa-kerwan-off/shots/frame1440.png'
shutil.copyfile(shot,outputs/'skill-points-ra-preview.png')
shutil.copyfile(root/'qa-final-normal-route/shots/frame990.png',outputs/'skill-points-ra-pausa.png')
report={
    'version':'0.1.2','game':'rac1-ntsc','disc':'SCUS_971.99',
    'officialAchievementCount':178,'newSkillPoints':148,'originalSkillPoints':30,'points':667,
    'hostTestGroups':5,'hostTestsPassed':True,'nativeLoadVerified':True,
    'nativeUiVerified':True,'controlledNativeUnlockVerified':253912,
    'controlledUnlockMethod':'Native Item_Give(Pyrocitor), isolated RAM/cards/profile; not a manual campaign unlock',
    'weaponXpCoexistenceVerified':True,'falseWeaponXpLevelEventsDuringUi':0,
    'nativeMenuEntryRouteVerified':'Pausa -> Premi (sotto Opzioni) -> Punti Stile',
    'normalCampaignControllerNavigationVerified':True,'pauseReopeningVerified':True,
    'pauseReopeningVerifiedVersion':'0.1.1',
    'kerwanTimeTrial':{'id':264462,'defaultEnabled':False,'nativeToggleOffVerified':True,
                      'nativeToggleOnVerified':True,'defaultActiveCount':177,'defaultActivePoints':657,
                      'previousUnlocksPreserved':True},
    'storyCompletionFlagDuringNativeProof':0,'qaPageOrSelectionWritesUsed':False,
    'sourceArchiveBuildVerified':'100% tests passed' in (root/'source-check.log').read_text(),
    'all178ManuallyCompleted':False,'accountCredentialsIncluded':False,'onlineUnlockSubmission':False,
    'nativeLevelSmokesVerifiedVersion':'0.1.1',
    'nativeLevelSmokes':[{k:v for k,v in row.items() if k!='log'} for row in levels],
    'evidence':{'hostTests':'work/skill-points-ra/test-final.log',
                'nativeUnlock':'work/skill-points-ra/qa-kerwan-off/data/openrac-rac1-ntsc.log',
                'nativeToggleOn':'work/skill-points-ra/qa-kerwan-on/data/openrac-rac1-ntsc.log',
                'uiPreview':'skill-points-ra-preview.png'},
    'nativeDllSha256':hashlib.sha256(dll).hexdigest(),
}
(outputs/'skill-points-ra-validation.json').write_text(json.dumps(report,indent=2)+'\n')
delivered=[native,source,outputs/'skill-points-ra-LEGGIMI.md',outputs/'skill-points-ra-preview.png',outputs/'skill-points-ra-pausa.png',outputs/'skill-points-ra-validation.json']
(outputs/'skill-points-ra-SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in delivered))
print(json.dumps({'zip':str(native),'bytes':native.stat().st_size,'source_bytes':source.stat().st_size,'native_levels_ready':sum(row['uiReady'] for row in levels)}))
