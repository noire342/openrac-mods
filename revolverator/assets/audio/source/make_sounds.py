"""Original, deterministic sound-design sketches for Revolverator review.

All source material is synthesized here. This is an audition pack, not game
integration. Loops are periodic mono PCM and can later be spatialized in-engine.
"""
from __future__ import annotations

import json
import math
from pathlib import Path
import subprocess
import wave
import zipfile

import numpy as np

SR = 48000
ROOT = Path(__file__).resolve().parent
OUT = ROOT / 'anteprima_v2'
FFMPEG = Path(r'C:\Program Files\Recordly\resources\app.asar.unpacked\node_modules\ffmpeg-static\ffmpeg.exe')
TAU = 2 * np.pi


def times(seconds):
    return np.arange(round(seconds * SR), dtype=np.float64) / SR


def bell(t, at, hz, decay, gain=1.0, bend=0.0):
    q = np.maximum(t - at, 0.0)
    attack = np.minimum(q / .0025, 1.0)
    env = (t >= at) * attack * np.exp(-q / decay)
    phase = TAU * (hz * q + bend * decay * (1 - np.exp(-q / decay)))
    return gain * env * np.sin(phase)


def noise_band(n, low, high, seed):
    rng = np.random.default_rng(seed)
    white = rng.standard_normal(n)
    spectrum = np.fft.rfft(white)
    freqs = np.fft.rfftfreq(n, 1 / SR)
    highpass = 1 / np.sqrt(1 + (low / np.maximum(freqs, 1.0)) ** 8)
    lowpass = 1 / np.sqrt(1 + (freqs / high) ** 8)
    spectrum *= highpass * lowpass
    x = np.fft.irfft(spectrum, n=n)
    return x / max(float(np.sqrt(np.mean(x*x))), 1e-8)


def window(t, start, duration, attack=.012, release=.05):
    u = t-start
    a = np.clip(u / max(attack, .0001), 0, 1)
    r = np.clip((duration-u) / max(release, .0001), 0, 1)
    return np.sin(a*np.pi/2)**2 * np.sin(r*np.pi/2)**2 * (u >= 0) * (u < duration)


def chirp(t, at, duration, f0, f1, gain, partial=1):
    q = np.clip(t-at, 0, duration)
    # Exponential glissando with continuous phase and no discontinuity.
    ratio = math.log(f1/f0)
    phase = TAU * f0 * duration / ratio * (np.exp(ratio * q / duration)-1) if abs(ratio) > 1e-9 else TAU*f0*q
    env = window(t, at, duration, .014, min(.085, duration*.38))
    return gain * env * np.sin(partial*phase)


def metallic(t, at, weight=1.0, pitch=1.0, tail=.26):
    freqs = (510, 820, 1288, 1940, 2835, 4070)
    gains = (.34, .27, .22, .14, .085, .045)
    out = np.zeros_like(t)
    for i, (f, g) in enumerate(zip(freqs, gains)):
        out += bell(t, at, f*pitch, tail*(.9+i*.045), weight*g, bend=4/(i+1))
    burst = noise_band(len(t), 850, 9000, int(1000*pitch)+13)
    q = np.maximum(t-at, 0)
    out += weight*.19*burst*(t>=at)*np.minimum(q/.0015, 1)*np.exp(-q/.012)
    return out


def motor(t, start, duration, base0, base1, gain, gear_hz=10):
    u = np.clip((t-start)/duration, 0, 1)
    sm = u*u*(3-2*u)
    inst = base0 + (base1-base0)*sm
    phi = TAU*np.cumsum(inst)/SR
    # Several inharmonic partials form a small geared motor rather than a sine beep.
    tone = (.48*np.sin(phi) + .23*np.sin(2*phi+.2) + .12*np.sin(3*phi+.7)
            + .08*np.sin(5*phi+1.1) + .06*np.sin(7*phi+.3))
    gear = .65 + .35*(.5+.5*np.sin(TAU*(gear_hz*t + .15*np.sin(TAU*1.2*t))))**5
    whirr = noise_band(len(t), 700, 4200, int(base1)+401)
    return gain*window(t, start, duration, .025, .09)*(tone*gear + .045*whirr)


def air(t, at, duration, f0, f1, gain, seed):
    noise = noise_band(len(t), f0, f1, seed)
    env = window(t, at, duration, .025, min(.22, duration*.7))
    # Amplitude movement suggests a short close-range mechanical swing.
    u = np.clip((t-at)/duration, 0, 1)
    return gain*noise*env*np.sin(np.pi*u)**.8


def thud(t, at, gain=1.0, low=95, decay=.16):
    return (bell(t, at, low, decay, gain*.60, bend=-32)
            + bell(t, at, low*1.63, decay*.65, gain*.22, bend=-20)
            + bell(t, at, low*2.42, decay*.35, gain*.11, bend=-10))


def ambience(x, wet=.08):
    # Short local reflections, restrained enough for close third-person combat.
    y = x.copy()
    for delay, level in ((.023,.52),(.041,.32),(.071,.18)):
        k = round(delay*SR)
        y[k:] += wet*level*x[:-k]
    return y


def finalize(x, peak_db=-4, looping=False):
    x = np.asarray(x, dtype=np.float64)
    x -= np.mean(x)
    if looping:
        # Match both value and first difference across the repeat boundary.
        # Correct gradually over the last 15 ms so no hard splice is audible.
        edge = min(round(.015*SR), len(x)//4)
        u = np.linspace(0,1,edge)
        correction = (2*x[0]-x[1])-x[-1]
        x[-edge:] += correction*u*u*(3-2*u)
    else:
        # Smooth boundaries; retain plenty of transient content after the first 2 ms.
        edge = min(round(.003*SR), len(x)//4)
        x[:edge] *= np.sin(np.linspace(0,np.pi/2,edge,endpoint=False))**2
        edge = min(round(.012*SR), len(x)//4)
        x[-edge:] *= np.cos(np.linspace(0,np.pi/2,edge,endpoint=True))**2
    peak = float(np.max(np.abs(x)))
    if peak:
        x *= 10**(peak_db/20)/peak
    return x


def loop_motor(seconds, speed, tone, seed):
    t = times(seconds)
    # Frequencies and their modulation are exact integer cycles per buffer.
    p = TAU*tone*t
    main = (.48*np.sin(p) + .25*np.sin(2*p+.22) + .13*np.sin(3*p+.6)
            + .07*np.sin(5*p+.4) + .035*np.sin(7*p+1))
    pulses = (.5+.5*np.sin(TAU*speed*t+.3))**4
    breath = 1 + .055*np.sin(TAU*.5*t)
    hiss = noise_band(len(t), 600, 3500, seed)
    # Periodic noise is intrinsically circular in the FFT; smooth its seam.
    edge = round(.06*SR)
    ramp = np.sin(np.linspace(0,np.pi/2,edge,endpoint=False))**2
    midpoint = .5*(hiss[:edge] + hiss[-edge:])
    hiss[:edge] = midpoint*(1-ramp) + hiss[:edge]*ramp
    hiss[-edge:] = hiss[-edge:]*ramp[::-1] + midpoint*(1-ramp[::-1])
    return breath*(main*(.78+.22*pulses) + .033*hiss)


def equip():
    t = times(.82)
    x = thud(t,.012,.32,180,.08)+metallic(t,.026,.38,1.05,.13)
    x += motor(t,.045,.54,105,390,.34,13)
    x += chirp(t,.07,.44,280,1090,.17)
    x += metallic(t,.165,.20,1.7,.09)+metallic(t,.45,.30,1.24,.12)
    return ambience(x,.11)


def attack():
    t=times(.52)
    x = motor(t,.0,.27,170,435,.50,19)
    x += air(t,.035,.29,500,5200,.16,2001)
    x += metallic(t,.018,.15,1.4,.08)
    return ambience(x,.07)


def capture():
    t=times(.68)
    x=thud(t,.016,.52,125,.10)+metallic(t,.027,.67,.82,.21)
    x+=metallic(t,.105,.32,1.22,.16)+motor(t,.045,.40,245,365,.30,15)
    x+=air(t,.02,.24,1200,8200,.075,2002)
    return ambience(x,.11)


def accelerating():
    t=times(2.1)
    x=motor(t,0,2.02,110,250,.46,7)
    x+=chirp(t,.10,1.85,260,620,.07,2)
    n=noise_band(len(t), 1200,7200,2003)
    env=window(t,.08,1.84,.08,.15)
    u=np.clip((t-.08)/1.84,0,1)
    x+=.03*n*env*u*(.5+.5*np.sin(TAU*(4*t+1.4*t*t)))**4
    return ambience(x,.06)


def release():
    t=times(.72)
    x=metallic(t,.015,.38,1.18,.14)+thud(t,.035,.35,115,.09)
    x+=air(t,.045,.43,330,4500,.19,2004)
    x+=chirp(t,.045,.36,600,180,.16)+motor(t,.01,.24,310,95,.24,14)
    return ambience(x,.10)


def impact_heavy():
    t=times(.65)
    x=thud(t,.006,.92,105,.13)+metallic(t,.009,.76,.72,.25)
    x+=air(t,.0,.09,850,8200,.13,2005)
    return ambience(x,.14)


def impact_throw():
    t=times(.86)
    x=thud(t,.008,.95,77,.19)+metallic(t,.02,.48,.64,.26)
    x+=air(t,.01,.21,270,3100,.18,2006)+metallic(t,.16,.14,1.55,.14)
    return ambience(x,.16)


def explosion(full):
    t=times(1.65 if full else 1.12)
    x=thud(t,.008,1.08 if full else .80,62 if full else 78,.28 if full else .18)
    x+=metallic(t,.015,.48 if full else .33,1.48,.31)
    x+=air(t,.012,.78 if full else .52,110,5000,.38 if full else .25,2007 if full else 2008)
    x+=chirp(t,.045,.42 if full else .32,380,110,.21 if full else .15)
    x+=bell(t,.027,1170,.18,.08 if full else .06)+bell(t,.05,1870,.22,.07 if full else .04)
    if full:
        x+=thud(t,.18,.25,53,.25)+air(t,.19,.7,95,1100,.12,2009)
    return ambience(x,.19)


def dry_fire():
    t=times(.32)
    x=metallic(t,.007,.38,1.05,.07)+motor(t,.009,.105,140,175,.21,10)
    x+=chirp(t,.04,.13,245,175,.09)+metallic(t,.145,.26,.75,.055)
    return ambience(x,.06)


def ammo():
    t=times(.56)
    x=metallic(t,.02,.38,1.7,.13)+metallic(t,.105,.24,2.05,.13)
    x+=chirp(t,.03,.32,370,700,.09)+thud(t,.016,.13,190,.065)
    return ambience(x,.08)


def upgrade(gold):
    t=times(1.18 if gold else 1.00)
    x=metallic(t,.02,.26,1.25,.18)+motor(t,.045,.53,125,360,.23,12)
    x+=chirp(t,.10,.62,290,630 if gold else 525,.12)
    for at, hz in ((.18,590),(.32,742),(.47,880),(.63,1175 if gold else 990)):
        x+=bell(t,at,hz,.16,.11 if gold else .085)
    x+=metallic(t,.55,.27,1.6 if gold else 1.32,.18)
    return ambience(x,.12)


CATALOG = [
 ('01_costruzione', 'Selezione e costruzione', 'Il corpo scatta in posizione, poi la punta si avvita.', equip, -4.5, False),
 ('02_riposo_loop', 'Rotore a riposo', 'Motore molto discreto: solo quando ci sono munizioni.', lambda:loop_motor(2,.5,72,21), -18, True),
 ('03_attacco', 'Attacco', 'Scatto rapido della trivella quando premi Cerchio.', attack, -4.5, False),
 ('04_aggancio', 'Aggancio del nemico', 'Presa metallica e avvio del motore sotto carico.', capture, -4.0, False),
 ('05_rotazione_loop', 'Nemico in rotazione', 'Motore ciclico da usare finché il nemico gira.', lambda:loop_motor(2,4,140,23), -10, True),
 ('06_rotazione_veloce_loop', 'Rotazione veloce', 'Variante più veloce e tesa, da collegare alla velocità effettiva.', lambda:loop_motor(2,6.5,225,24), -9, True),
 ('07_accelerazione_demo', 'Esempio di accelerazione', 'Dimostrazione dell’aumento continuo di velocità; non è un loop.', accelerating, -7, False),
 ('08_lancio', 'Lancio', 'Sgancio, spinta e passaggio rapido del nemico.', release, -4, False),
 ('09_colpo_pesante', 'Colpo a un nemico pesante', 'Impatto secco, per boss e nemici che non possono essere sollevati.', impact_heavy, -3.5, False),
 ('10_impatto_nemico', 'Impatto del nemico lanciato', 'Urto corposo con una breve risonanza metallica.', impact_throw, -3.5, False),
 ('11_oro_esplosione_base', 'Esplosione dorata - carica breve', 'Esplosione compatta dopo una rotazione breve.', lambda:explosion(False), -4, False),
 ('12_oro_esplosione_massima', 'Esplosione dorata - carica massima', 'Esplosione più profonda e ampia, dopo una rotazione lunga.', lambda:explosion(True), -2.5, False),
 ('13_senza_munizioni', 'Senza munizioni', 'Breve tentativo del motore e scatto all’indietro.', dry_fire, -7, False),
 ('14_munizioni_raccolte', 'Munizioni raccolte', 'Piccolo incastro meccanico, pensato per non coprire il gioco.', ammo, -9, False),
 ('15_acquisto_dorato', 'Acquisto della variante dorata', 'Costruzione meccanica con una breve coda più luminosa.', lambda:upgrade(True), -5, False),
 ('16_aumento_livello', 'Aumento di livello', 'Breve accento per il Rotatore, opzionale con Weapon Experience.', lambda:upgrade(False), -6, False),
]


def write_pcm(path, samples):
    # 24-bit signed little-endian mono, preserving spatialization options.
    samples = np.clip(samples, -1, 1)
    bits = np.rint(samples * (2**23-1)).astype(np.int32)
    packed = np.empty((len(bits), 3), dtype=np.uint8)
    packed[:,0] = bits & 255
    packed[:,1] = (bits >> 8) & 255
    packed[:,2] = (bits >> 16) & 255
    with wave.open(str(path),'wb') as f:
        f.setnchannels(1); f.setsampwidth(3); f.setframerate(SR); f.writeframes(packed.tobytes())


def main():
    if OUT.exists():
        raise SystemExit('Refusing to overwrite an existing audition pack')
    if not FFMPEG.is_file():
        raise SystemExit('The configured local FFmpeg executable is missing')
    (OUT/'wav').mkdir(parents=True)
    (OUT/'mp3').mkdir()
    rendered=[]
    audition=[]
    lines=['# Rotatore - anteprima suoni', '',
           'Bozze originali sintetizzate per ascolto e approvazione. I file non sono ancora integrati nella mod.', '',
           'Le tracce loop sono progettate per essere ripetute; la rotazione va collegata alla velocità reale in gioco.', '',
           'I WAV sono mono 48 kHz / 24 bit; gli MP3 sono copie per ascolto.', '', '## Tracce', '']
    cursor=0.0
    for i,(name,title,description,generate,peak_db,looping) in enumerate(CATALOG):
        sound=finalize(generate(),peak_db,looping)
        if not np.isfinite(sound).all() or len(sound)<SR//10:
            raise ValueError('Invalid audio: '+name)
        wav=OUT/'wav'/f'{name}.wav'; mp3=OUT/'mp3'/f'{name}.mp3'
        write_pcm(wav,sound)
        # Longer listening examples for loops; WAVs remain the exact loop material.
        part = np.tile(sound,2) if looping else sound
        if looping:
            fade=min(round(.12*SR),len(part)//4)
            part=part.copy();part[:fade]*=np.sin(np.linspace(0,np.pi/2,fade))**2
            part[-fade:]*=np.cos(np.linspace(0,np.pi/2,fade))**2
        subprocess.run([str(FFMPEG),'-hide_banner','-loglevel','error','-f','f32le','-ar',str(SR),
                        '-ac','1','-i','pipe:0','-map_metadata','-1','-codec:a','libmp3lame',
                        '-b:a','192k','-y',str(mp3)], input=part.astype('<f4').tobytes(),check=True)
        audition.append(part)
        seam=abs(float(sound[0]-sound[-1]))
        rendered.append({'id':name,'title':title,'description':description,'wav':f'wav/{name}.wav',
                         'mp3':f'mp3/{name}.mp3','duration_seconds':round(len(sound)/SR,3),
                         'loop':looping,'peak_dbfs':round(20*np.log10(max(np.max(np.abs(sound)),1e-12)),2),
                         'rms_dbfs':round(20*np.log10(max(np.sqrt(np.mean(sound*sound)),1e-12)),2),
                         'loop_boundary_step':round(seam,8) if looping else None,
                         'montage_start_seconds':round(cursor,2)})
        lines.append(f'{i+1:02}. **{title}** ({cursor:.1f} s): {description} [MP3](mp3/{name}.mp3) · [WAV](wav/{name}.wav)')
        cursor+=len(part)/SR+1.05
    pause=np.zeros(round(1.05*SR))
    montage=np.concatenate([p for s in audition for p in (s,pause)])
    write_pcm(OUT/'ascolto_completo.wav',montage)
    subprocess.run([str(FFMPEG),'-hide_banner','-loglevel','error','-i',str(OUT/'ascolto_completo.wav'),
                    '-map_metadata','-1','-codec:a','libmp3lame','-b:a','192k','-y',str(OUT/'ascolto_completo.mp3')],check=True)
    lines += ['', 'Le versioni dorate breve e massima rappresentano gli estremi: l’effetto finale potrà variare gradualmente.',
              'I volumi sono indicativi: il mix definitivo dipenderà dall’ascolto nel gioco.', '']
    (OUT/'LEGGIMI.md').write_text('\n'.join(lines),encoding='utf-8')
    (OUT/'tracce.json').write_text(json.dumps({'sample_rate':SR,'format':'mono PCM 24-bit',
                     'prototype':True,'source':'original procedural synthesis','tracks':rendered},indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    zip_path=ROOT/'rotatore-anteprima-suoni-v2.zip'
    with zipfile.ZipFile(zip_path,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=8) as z:
        for path in sorted(OUT.rglob('*')):
            if path.is_file():z.write(path,path.relative_to(OUT).as_posix())
    print(json.dumps({'tracks':len(rendered),'montage_seconds':round(len(montage)/SR,2),
                      'directory':str(OUT),'zip':str(zip_path),'zip_bytes':zip_path.stat().st_size,
                      'tracks_summary':rendered},indent=2,ensure_ascii=False))


if __name__=='__main__':
    main()
