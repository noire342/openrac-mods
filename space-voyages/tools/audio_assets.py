#!/usr/bin/env python3
"""Append PRIVATE original-disc audio to a NEW asset pack, at neutral gain.

The offline C++ helper links the existing OpenRAC989snd/SPU/ADSR code; it
never opens an audio device or game. Commercial PCM stays in private work.
"""
from __future__ import annotations
import argparse,json,math,shutil,struct,subprocess,wave
from pathlib import Path
from asset_formats import bounded,digest
from build_assets import Source
from flight_effects_assets import check_inventory,file_inventory

ROLES=('music','engine_531','engine_532','engine_533','laser_532','laser_533',
       'rock_hit','rock_destroyed','ship_hit','ship_destroyed',
       'turbo_531','turbo_532','turbo_533')
NEUTRAL_GAIN_POLICY='Original bank/grain/request/ADSR chain rendered offline unchanged; all13role gains and pitches are1. No added music attenuation, compensation, peak normalisation or mix option; user output settings remain authoritative.'

def level_sections(source,level):
    data=source.level_data(level);result={}
    for number,name in ((1,'bank'),(2,'index'),(10,'core')):
        at,size=struct.unpack_from('<II',data,number*8);raw=bounded(data,at,size)
        result[name]=source.wad(raw) if raw[:3]==b'WAD' else raw
    return result

def bank_info(raw):
    if struct.unpack_from('<II',raw)!=(3,2):raise ValueError('Expected original989snd two-chunk bank')
    bo,bs,so,ss=struct.unpack_from('<4I',raw,8);block=bounded(raw,bo,bs)
    bounded(raw,so,ss)
    if block[:4]!=b'SBlk' or struct.unpack_from('<I',block,4)[0]!=1:raise ValueError('Expected nativeSBlk v1')
    count=struct.unpack_from('<h',block,0x16)[0];first_sound,first_grain=struct.unpack_from('<II',block,0x1c)
    if not(0<count<4096):raise ValueError('Invalid native bank sound count')
    return {'block':block,'sound_count':count,'first_sound':first_sound,'first_grain':first_grain,
            'sample_offset':so,'sample_bytes':ss,'bank_sha256':digest(raw),'sample_chunk_sha256':digest(bounded(raw,so,ss))}

def sound_info(raw,sid):
    bank=bank_info(raw);block=bank['block']
    if not(0<=sid<bank['sound_count']):raise ValueError('Original sound id outside bank')
    record=bounded(block,bank['first_sound']+sid*12,12)
    volume,group,pan,count,limit,flags,offset=struct.unpack('<bbhBbHI',record);grains=[]
    for k in range(count):
        g=bounded(block,bank['first_grain']+offset+k*40,40);kind,delay=struct.unpack_from('<Ii',g);data=g[8:]
        grain={'type':kind,'delay_240hz':delay,'parameters_hex':data.hex()}
        if kind in (1,9):
            tone=struct.unpack_from('<bbbbhbbbbHHHII',data)
            grain['tone']={'priority':tone[0],'volume':tone[1],'center_note':tone[2],'center_fine':tone[3],
                'pan':tone[4],'map_low':tone[5],'map_high':tone[6],'bend_down':tone[7],'bend_up':tone[8],
                'adsr1':tone[9],'adsr2':tone[10],'flags':tone[11],'sample_offset':tone[12]}
        grains.append(grain)
    return {'bank_sound_id':sid,'sound_volume':volume,'volume_group':group,'pan':pan,'instance_limit':limit,
            'flags':flags,'grain_count':count,'grains':grains}

def mapped_sound(sections,oclass,local):
    index,core=sections['index'],sections['core'];count,table=struct.unpack_from('<II',index,0x18)
    remap=struct.unpack_from('<I',index,0x70)[0]
    if not remap:raise ValueError('Native class sound remap missing')
    for ci in range(count):
        blob,cl=struct.unpack_from('<ii',index,table+ci*32)
        if cl!=oclass:continue
        offset,n=struct.unpack_from('<hh',index,remap+8+ci*4)
        if not(0<=local<n):raise ValueError('Original local sound slot absent')
        sid=struct.unpack_from('<H',index,remap+offset+local*4)[0]
        if blob<=0 or local>=core[blob+13]:raise ValueError('Native sound definition absent')
        defs=struct.unpack_from('<i',core,blob+0x28)[0]
        near,far,volfar,volnear,bendlo,bendhi,looped,flags,_,_=struct.unpack_from('<ffiiiiBBHI',core,blob+defs+local*32)
        result=sound_info(sections['bank'],sid)
        result.update({'native_level':13,'native_class':oclass,'local_sound_slot':local,
            'definition':{'near':near,'far':far,'volume_far':volfar,'volume_near':volnear,'bend_low':bendlo,'bend_high':bendhi,'looped':looped,'flags':flags},
            'render_request_volume':volnear,'render_request_bend':int((bendlo+bendhi)/2)})
        return result
    raise ValueError('Native class absent from original sound remap')

def music_part(source,level,track):
    lba,_=struct.unpack_from('<II',source.toc,source.version.level_table+level*8)
    source.file.seek(lba*2048);header=source.file.read(0x2434)
    sector=struct.unpack_from('<I',header,0x148+track*4)[0]
    source.file.seek(sector*2048);vag=source.file.read(48)
    if vag[:4]!=b'VAGp':raise ValueError('Original requested music track is not VAG')
    size,rate=struct.unpack_from('>II',vag,12)
    if size%16 or not(0<size<64*1024*1024) or not(8000<=rate<=96000):raise ValueError('Invalid native music stream')
    body=source.file.read(size)
    if len(body)!=size:raise ValueError('Truncated original music VAG')
    return vag+body,{'native_level':level,'track_index':track,'sector':sector,'sample_rate':rate,
        'vag_bytes':size+48,'vag_sha256':digest(vag+body),'embedded_name':vag[32:48].split(b'\0')[0].decode('ascii','replace')}

def call_tool(tool,args):
    result=subprocess.run([str(tool),*map(str,args)],capture_output=True,text=True,check=False)
    if result.returncode:raise ValueError('Original audio offline render failed: '+result.stdout+result.stderr)

def write_wav(path,pcm,rate,channels):
    with wave.open(str(path),'wb') as out:out.setnchannels(channels);out.setsampwidth(2);out.setframerate(rate);out.writeframes(pcm)

def prepare_audio(source,tool,stage):
    if stage.exists():raise ValueError('Never overwrite an audio staging folder')
    stage.mkdir(parents=True);sections=level_sections(source,13);(stage/'gemlik.bank.bin').write_bytes(sections['bank'])
    clips={};native={};bank_sources={'gemlik':{'bytes':len(sections['bank']),'sha256':digest(sections['bank']),'sound_count':bank_info(sections['bank'])['sound_count']}}
    selections=(('engine',69,0,True),('turbo',69,1,True),('laser',69,3,False),
                ('rock_hit',212,0,False),('rock_destroyed',1412,0,False),('ship_destroyed',69,6,False))
    for name,oclass,local,loop in selections:
        info=mapped_sound(sections,oclass,local);output=stage/(name+'.pcm.bin');meta=stage/(name+'.render.json')
        call_tool(tool,['bank',stage/'gemlik.bank.bin',info['bank_sound_id'],info['render_request_volume'],info['render_request_bend'],int(loop),output,meta])
        render=json.loads(meta.read_text());pcm=output.read_bytes()
        if not render['nonzero_samples'] or render['peak']==0:raise ValueError('Original audio rendered silence')
        if len(pcm)!=render['frames']*render['channels']*2:raise ValueError('Offline PCM byte count mismatch')
        clips[name]={'pcm':pcm,**render};native[name]={**info,'render':render,'pcm_sha256':digest(pcm)}
        write_wav(stage/(name+'.wav'),pcm,render['sample_rate'],render['channels'])
    # A collision is a new scene-local event. Reuse the verified original
    # asteroid impact sound, not a falsely named unverified fighter slot.
    clips['ship_hit']=clips['rock_hit'];native['ship_hit']={'reuse':'rock_hit','render':native['rock_hit']['render'],
        'pcm_sha256':native['rock_hit']['pcm_sha256'],'adaptation':'Original asteroid212impact sample reused as mod hull-collision feedback'}
    native['ship_destroyed']['source']='JetFighterUpdateL13 fatal hit and PlayerFighterFlight fatal collision call spawn_explosion with local sound6. Original PAL MIPS at2bc570/2bc160/2bc1f0 sets t2=6; shared explosion forwards that integer to Moby_PlaySound.'
    music=[];sources=[]
    for track in (2,3):
        raw,info=music_part(source,13,track);vag=stage/f'music{track}.vag';vag.write_bytes(raw);output=stage/f'music{track}.pcm.bin';meta=stage/f'music{track}.render.json'
        call_tool(tool,['vag',vag,output,meta]);render=json.loads(meta.read_text());music.append((output.read_bytes(),render));sources.append(info)
    if music[0][1]['sample_rate']!=music[1][1]['sample_rate']:raise ValueError('Original music intro/loop rates differ')
    pcm=music[0][0]+music[1][0];intro_frames=music[0][1]['frames'];frames=intro_frames+music[1][1]['frames']
    clips['music']={'pcm':pcm,'sample_rate':music[0][1]['sample_rate'],'channels':1,'frames':frames,
        'loop_begin_frame':intro_frames,'loop_end_frame':frames}
    native['music']={'label':'Gemlik requested track2','source':'JetFighterUpdateL13 calls MusicRequestTrack(2,4); music_StartTrackBody queues track+1',
        'parts':sources,'intro_frames':intro_frames,'loop_begin_frame':intro_frames,'loop_end_frame':frames,
        'pcm_sha256':digest(pcm),'decode':'Original OpenRAC roundedSPU ADPCM decoder; VAG mono sample rate preserved, independent VAG histories reset as native file loader',
        'caveat':'No guessed commercial soundtrack title. This is Gemlik requested track2, not a claimed original free-space exploration score.'}
    (stage/'audio-source-audit.json').write_text(json.dumps({'disc':source.identity,'banks':bank_sources,'clips':native},indent=2)+'\n',encoding='ascii')
    return clips,native

def audio_records(clips):
    aliases={'music':'music','rock_hit':'rock_hit','rock_destroyed':'rock_destroyed','ship_hit':'ship_hit','ship_destroyed':'ship_destroyed'}
    aliases.update({f'engine_{cl}':'engine' for cl in (531,532,533)})
    aliases.update({f'turbo_{cl}':'turbo' for cl in (531,532,533)})
    aliases.update({f'laser_{cl}':'laser' for cl in (532,533)})
    records=['format 1'];payloads={}
    for role in ROLES:
        name=aliases[role];clip=clips[name];filename='audio/'+name+'.pcm.bin';payloads[filename]=clip['pcm']
        records.append(f'clip {role} {filename} {clip["sample_rate"]} {clip["channels"]} {clip["frames"]} {clip.get("loop_begin_frame",0)} {clip.get("loop_end_frame",0)}')
        # Neutral transfer: preserve the native library render and the user's
        # output settings. No extra attenuation, boost or waveform normalisation.
        records.append(f'gain {role} 1')
        records.append(f'pitch {role} 1')
    return records,payloads,aliases

def build(base,iso,editor,tool,region,out,stage):
    base,out=base.resolve(),out.resolve()
    if out.exists():raise ValueError('Never overwrite an audio pack')
    if out.is_relative_to(base) or base.is_relative_to(out):raise ValueError('Input/output paths overlap')
    old_inventory=json.loads((base/'inventory.json').read_text());check_inventory(base,old_inventory)
    old_provenance=(base/'provenance.json').read_bytes();old_inventory_raw=(base/'inventory.json').read_bytes()
    source=Source(iso,editor,region)
    try:
        if json.loads(old_provenance)['disc']!=source.region:raise ValueError('Cannot mix native audio and scene regions')
        clips,native=prepare_audio(source,tool,stage);identity=source.identity
    finally:source.close()
    records,payloads,aliases=audio_records(clips)
    logical_bytes=sum(len(clips[aliases[role]]['pcm']) for role in ROLES)
    if logical_bytes>80*1024*1024:raise ValueError('Decoded audio exceeds planned80MiBbudget')
    shutil.copytree(base,out);(out/'audio').mkdir()
    (out/'base-flightfx-03.provenance.json').write_bytes(old_provenance);(out/'base-flightfx-03.inventory.json').write_bytes(old_inventory_raw)
    for name,raw in payloads.items():(out/name).write_bytes(raw)
    (out/'audio.tsv').write_text('\n'.join(records)+'\n',encoding='ascii')
    extension={'format':1,'private':True,'region':identity['region'],'disc_identity':identity,'iso':str(iso.resolve()),'editor':str(editor.resolve()),
        'base':str(base),'base_inventory':old_inventory,'base_provenance_sha256':digest(old_provenance),'base_inventory_sha256':digest(old_inventory_raw),
        'offline_renderer':{'file':str(tool.resolve()),'sha256':digest(tool.read_bytes()),'implementation':'Read-only OpenRAC SoundPlayer/ADSR/4tapGaussianSPU/pitch/grainVM; no game or audio device'},
        'clips':native,'roles':aliases,'logical_audio_bytes':logical_bytes,'unique_pcm_bytes':sum(map(len,payloads.values())),
        'appearance_policy':'All3cinematic ship appearances use verified Gemlik fighter69engine/turbo; no invented claim of separate531/532/533native engine banks.',
        'loop_policy':'Full rendered startup retained; loop points correspond to original repeating-tone source phase after native pitch script stabilises.48koutput markers quantised to one output frame; no arbitrary seamless-loop edit.',
        'mix_policy':'neutral',
        'gain_policy':NEUTRAL_GAIN_POLICY,
        'source_policy':'Only Gemlik level13 combat bank and requested Gemlik music2/3. No ambient drone, transition-bank cue, or synthetic respawn sound. Respawn role intentionally absent: no original Gemlik respawn one-shot verified.',
        'zero_rendered_unsupported_grains':True}
    (out/'audio-extension.json').write_text(json.dumps(extension,indent=2)+'\n',encoding='ascii')
    provenance=json.loads(old_provenance);provenance['scene_audio']=extension
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='ascii')
    (out/'inventory.json').write_text(json.dumps(file_inventory(out),indent=2)+'\n',encoding='ascii')
    return verify(out)

def neutralize(base,out):
    """Derive NEW05 from frozen04; only four existing metadata files change."""
    base,out=base.resolve(),out.resolve()
    if out.exists():raise ValueError('Never overwrite an audio pack')
    if out.is_relative_to(base) or base.is_relative_to(out):raise ValueError('Input/output paths overlap')
    verify(base);before=file_inventory(base);original_inventory_sha=digest((base/'inventory.json').read_bytes())
    extension=json.loads((base/'audio-extension.json').read_text())
    if extension.get('mix_policy')=='neutral':raise ValueError('Input already has neutral audio gain')
    old_manifest=(base/'audio.tsv').read_bytes();lines=old_manifest.decode('ascii').splitlines();records=[]
    for line in lines:
        row=line.split()
        if row[0]=='gain':line=f'gain {row[1]} 1'
        records.append(line)
    extension['mix_policy']='neutral'
    extension['gain_policy']=NEUTRAL_GAIN_POLICY
    extension['mix_parent']={'pack':str(base),'audio_tsv_sha256':digest(old_manifest),
        'audio_extension_sha256':digest((base/'audio-extension.json').read_bytes()),
        'inventory_sha256':digest((base/'inventory.json').read_bytes()),
        'change':'Remove only the previous music gain0.4; all gains now1. Every PCM, mesh, texture, loop point and remaining metadata payload stays byte-identical.'}
    provenance=json.loads((base/'provenance.json').read_text());provenance['scene_audio']=extension
    shutil.copytree(base,out)
    (out/'audio.tsv').write_text('\n'.join(records)+'\n',encoding='ascii')
    (out/'audio-extension.json').write_text(json.dumps(extension,indent=2)+'\n',encoding='ascii')
    (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n',encoding='ascii')
    inventory=file_inventory(out)
    changed={name for name in before if inventory[name]!=before[name]}
    if set(inventory)!=set(before) or changed!={'audio.tsv','audio-extension.json','provenance.json'}:
        raise ValueError('Neutral gain revision changed more than the permitted metadata')
    (out/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n',encoding='ascii')
    result=verify(out);result['parent_pack_unchanged']=file_inventory(base)==before and digest((base/'inventory.json').read_bytes())==original_inventory_sha
    if not result['parent_pack_unchanged']:raise ValueError('Original04 pack changed')
    result['all_pcm_and_visual_payloads_byte_identical_to_parent']=True
    result['changed_existing_files']=sorted(changed|{'inventory.json'})
    return result

def verify(root):
    root=root.resolve();e=json.loads((root/'audio-extension.json').read_text());base=Path(e['base'])
    check_inventory(root,json.loads((root/'inventory.json').read_text()));check_inventory(base,e['base_inventory'])
    for name,expected in e['base_inventory'].items():
        if name not in ('provenance.json','inventory.json') and digest((root/name).read_bytes())!=expected['sha256']:raise ValueError('Existing visual/physics payload changed')
    if digest((root/'base-flightfx-03.provenance.json').read_bytes())!=e['base_provenance_sha256'] or digest((root/'base-flightfx-03.inventory.json').read_bytes())!=e['base_inventory_sha256']:
        raise ValueError('Original visual-pack metadata changed')
    roles={};gains={};pitches={};lines=(root/'audio.tsv').read_text().splitlines()
    if not lines or lines[0]!='format 1':raise ValueError('Unsupported audio manifest version')
    for line in lines[1:]:
        row=line.split()
        if not row:raise ValueError('Empty audio manifest record')
        if row[0] in ('gain','pitch'):
            target=gains if row[0]=='gain' else pitches
            if len(row)!=3 or row[1] in target or row[1] not in ROLES:raise ValueError('Bad audio profile')
            value=float(row[2])
            if not math.isfinite(value) or value<0 or value>4 or (row[0]=='pitch' and value<=0):raise ValueError('Invalid audio gain/pitch')
            target[row[1]]=value;continue
        if row[0]!='clip' or len(row)!=8 or row[1] in roles or row[1] not in ROLES:raise ValueError('Bad audio clip record')
        role,path=row[1:3];rate,channels,frames,begin,end=map(int,row[3:]);p=(root/path).resolve()
        if ':' in path or '\\' in path or path.startswith('/') or '..' in Path(path).parts or not p.is_relative_to(root) or rate<8000 or rate>96000 or channels not in(1,2) or not(0<=begin<(end or frames)<=frames):raise ValueError('Invalid decoded audio dimensions/loop')
        raw=p.read_bytes()
        if len(raw)!=frames*channels*2 or not any(raw):raise ValueError('Invalid/silent PCM data')
        source=e['clips'][e['roles'][role]]
        if digest(raw)!=source['pcm_sha256']:raise ValueError('Original rendered PCM changed')
        render=source.get('render')
        if render and (rate,channels,frames,begin,end)!=(render['sample_rate'],render['channels'],render['frames'],render.get('loop_begin_frame',0),render.get('loop_end_frame',0)):
            raise ValueError('Original render rate/frames/loop metadata changed')
        if role=='music' and (channels!=1 or frames!=source['loop_end_frame'] or begin!=source['loop_begin_frame'] or end!=source['loop_end_frame'] or rate!=source['parts'][0]['sample_rate']):
            raise ValueError('Original music intro/loop metadata changed')
        roles[role]={'rate':rate,'channels':channels,'frames':frames,'loop':[begin,end],'bytes':len(raw)}
    if set(roles)!=set(ROLES) or set(gains)!=set(ROLES) or set(pitches)!=set(ROLES):raise ValueError('Missing semantic audio role/profile')
    if e.get('mix_policy')=='neutral' and (any(value!=1 for value in gains.values()) or any(value!=1 for value in pitches.values())):
        raise ValueError('Neutral audio pack must preserve gain1 and pitch1 for every role')
    return {'verified':True,'output':str(root),'region':e['region'],'clips':len(roles),'unique_pcm_bytes':e['unique_pcm_bytes'],
        'logical_audio_bytes':e['logical_audio_bytes'],'all_original_visual_payloads_unchanged':True,'audio_tsv_sha256':digest((root/'audio.tsv').read_bytes()),'roles':roles}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','iso','editor','tool','out','stage','neutral-from'):p.add_argument('--'+name,type=Path)
    p.add_argument('--region',choices=('pal','ntsc','auto'),default='auto');p.add_argument('--verify',type=Path);a=p.parse_args()
    if a.verify:result=verify(a.verify)
    elif a.neutral_from and a.out:result=neutralize(a.neutral_from,a.out)
    elif all((a.base,a.iso,a.editor,a.tool,a.out,a.stage)):result=build(a.base,a.iso,a.editor,a.tool,a.region,a.out,a.stage)
    else:p.error('Provide --base --iso --editor --tool --out --stage, --neutral-from --out, or --verify')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
