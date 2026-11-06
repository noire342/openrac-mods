"""Generate additional preview cues for the full-charge release behavior."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import wave
import zipfile
import numpy as np
import make_sounds as synth

ROOT=Path(__file__).resolve().parent
WORK=ROOT.parent
BASE=ROOT/'anteprima_v2'
OUT=ROOT/'audizione-completa'
BUNDLE=WORK/'revolverator-audio-runtime-20261105'
HANDOFF=WORK/'revolverator-audio-handoff.json'

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def early_release():
    t=synth.times(.62)
    x=synth.metallic(t,.008,.28,1.12,.10)
    x+=synth.motor(t,.012,.38,235,74,.42,9)
    x+=synth.chirp(t,.03,.31,410,125,.11)
    x+=synth.metallic(t,.255,.13,.82,.11)
    return synth.ambience(x,.06)

def ground_contact():
    t=synth.times(.46)
    x=synth.thud(t,.012,.38,92,.11)
    dust=synth.noise_band(len(t),130,2600,2039)
    q=np.maximum(t-.01,0)
    x+=.075*dust*(t>=.01)*np.minimum(q/.004,1)*np.exp(-q/.055)
    x+=synth.metallic(t,.018,.09,.62,.09)
    return synth.ambience(x,.08)

def charge_ready():
    t=synth.times(.57)
    x=synth.metallic(t,.015,.17,1.55,.12)
    x+=synth.bell(t,.025,875,.13,.14)
    x+=synth.bell(t,.078,1170,.15,.105)
    x+=synth.chirp(t,.025,.16,480,760,.07)
    return synth.ambience(x,.08)

EXTRAS=[
 ('17_sgancio_anticipato','early_release','Sgancio anticipato','Il motore cala e la presa si apre, senza suono di lancio.',early_release,-7,.70),
 ('18_contatto_terra','ground_contact','Contatto a terra','Un appoggio leggero quando il nemico tocca terra.',ground_contact,-9,.65),
 ('19_carica_pronta','charge_ready','Carica completa','Segnale breve, una sola volta quando la barra si riempie.',charge_ready,-10,.65),
]

def write_meta(path,obj):
    tmp=path.with_name(path.name+'.tmp')
    tmp.write_text(json.dumps(obj,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    tmp.replace(path)

def main():
    if OUT.exists():raise SystemExit('Refusing to overwrite the listening pack')
    before=json.loads(HANDOFF.read_text(encoding='utf-8'))
    manifest_path=BUNDLE/'manifest.json'
    if before['status']!='partial_pending_additional_cues' or before['manifest_sha256']!=digest(manifest_path):
        raise SystemExit('The initial handoff changed; refusing to overwrite it')
    (OUT/'mp3').mkdir(parents=True);(OUT/'wav').mkdir()
    for kind in ('mp3','wav'):
        for file in (BASE/kind).glob('*'):shutil.copy2(file,OUT/kind/file.name)
    base=json.loads((BASE/'tracce.json').read_text(encoding='utf-8'))
    cues=json.loads(manifest_path.read_text(encoding='utf-8'))
    extra_tracks=[]
    for name,key,title,description,fn,peak,gain in EXTRAS:
        x=synth.finalize(fn(),peak)
        wav=OUT/'wav'/f'{name}.wav';mp3=OUT/'mp3'/f'{name}.mp3'
        synth.write_pcm(wav,x)
        subprocess.run([str(synth.FFMPEG),'-hide_banner','-loglevel','error','-i',str(wav),
                        '-map_metadata','-1','-codec:a','libmp3lame','-b:a','192k','-y',str(mp3)],check=True)
        target=BUNDLE/'wav'/f'{key}.wav'
        if target.exists():raise ValueError('Extra cue already exists: '+key)
        shutil.copy2(wav,target)
        cue={'id':key,'path':str(target.resolve()),'relative_path':target.relative_to(BUNDLE).as_posix(),
          'sha256':digest(target),'sample_rate':synth.SR,'channels':1,'bits_per_sample':24,
          'sample_frames':len(x),'duration_seconds':round(len(x)/synth.SR,6),'loop':False,
          'loop_start_sample':None,'loop_end_sample_exclusive':None,'suggested_sound_play_volume':gain,
          'suggested_fade_in_ms':None,'suggested_fade_out_ms':None,'description_it':description,'use':'proposed_runtime'}
        cues['cues'].append(cue)
        extra_tracks.append({'id':name,'title':title,'description':description,'wav':f'wav/{name}.wav',
          'mp3':f'mp3/{name}.mp3','duration_seconds':len(x)/synth.SR,'loop':False})
    shutil.copy2(ROOT/'make_extra_cues.py',BUNDLE/'source'/'make_extra_cues.py')
    cues['sources']=['source/make_sounds.py','source/make_extra_cues.py']
    cues['pending']=[];cues['status']='ready_pending_user_listening'
    cues['runtime_notes'].extend([
      'Trigger early_release only for an incomplete charge; never combine it with the throw or explosion cue.',
      'Trigger ground_contact only for an actual validated landing; skip it if the event cannot be observed reliably.',
      'Trigger charge_ready once when crossing the full-charge threshold, not once per frame.',
      'For a first integration prefer one spin_loop voice with pitch updated smoothly; compare the alternate loop during user review.'
    ])
    # Preserve each existing WAV byte-for-byte; publish a fresh manifest atomically.
    write_meta(manifest_path,cues)
    before['status']=cues['status'];before['manifest_sha256']=digest(manifest_path);before['pending']=[]
    before['extra_source_script']=str((BUNDLE/'source'/'make_extra_cues.py').resolve())
    before['notes']='All 19 WAVs frozen. User listening approval and real-game mix validation are pending.'
    write_meta(HANDOFF,before)
    with wave.open(str(BASE/'ascolto_completo.wav'),'rb') as original:
        frames=original.readframes(original.getnframes()); assert original.getframerate()==synth.SR
    combined=OUT/'ascolto_completo.wav'
    with wave.open(str(combined),'wb') as w:
        w.setnchannels(1);w.setsampwidth(3);w.setframerate(synth.SR);w.writeframesraw(frames)
        cursor=len(frames)//3/synth.SR
        for track in extra_tracks:
            track['montage_start_seconds']=round(cursor,2)
            with wave.open(str(OUT/track['wav']),'rb') as source:
                raw=source.readframes(source.getnframes());w.writeframesraw(raw)
            w.writeframesraw(bytes(round(1.05*synth.SR)*3))
            cursor+=track['duration_seconds']+1.05
    subprocess.run([str(synth.FFMPEG),'-hide_banner','-loglevel','error','-i',str(combined),
                    '-map_metadata','-1','-codec:a','libmp3lame','-b:a','192k','-y',str(OUT/'ascolto_completo.mp3')],check=True)
    alltracks=base['tracks']+extra_tracks
    (OUT/'tracce.json').write_text(json.dumps({'sample_rate':synth.SR,'tracks':alltracks},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    readme=(BASE/'LEGGIMI.md').read_text(encoding='utf-8')
    readme+='\n## Segnali per la carica\n\n'
    for i,v in enumerate(extra_tracks,17):
        readme+=f"{i:02}. **{v['title']}** ({v['montage_start_seconds']:.1f} s): {v['description']} [MP3]({v['mp3']}) · [WAV]({v['wav']})\n"
    (OUT/'LEGGIMI.md').write_text(readme,encoding='utf-8')
    zip_path=ROOT/'rotatore-anteprima-suoni-completa.zip'
    with zipfile.ZipFile(zip_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=8) as z:
        for path in sorted(OUT.rglob('*')):
            if path.is_file():z.write(path,path.relative_to(OUT).as_posix())
    print(json.dumps({'handoff':str(HANDOFF.resolve()),'manifest_sha256':digest(manifest_path),
      'bundle':str(BUNDLE.resolve()),'new_cues':extra_tracks,'complete_montage_seconds':round(cursor,2),
      'preview_dir':str(OUT.resolve()),'preview_zip':str(zip_path.resolve())},indent=2,ensure_ascii=False))

if __name__=='__main__':main()
