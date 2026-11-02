"""Native bank mapping, PCM/loop and append-only pack contracts (synthetic data)."""
import io,json,os,struct,subprocess,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from audio_assets import ROLES,audio_records,bank_info,build,mapped_sound,music_part,neutralize,sound_info,verify
from asset_formats import digest
from flight_effects_assets import file_inventory

def bank(loop=False,kind=1):
    block=bytearray(0x3c+12+40);block[:4]=b'SBlk'
    struct.pack_into('<I',block,4,1);struct.pack_into('<hhh',block,0x16,1,1,1)
    struct.pack_into('<II',block,0x1c,0x3c,0x48)
    struct.pack_into('<bbhBbHI',block,0x3c,127,0,0,1,0,0,0)
    struct.pack_into('<Ii',block,0x48,kind,0)
    struct.pack_into('<bbbbhbbbbHHHII',block,0x50,1,127,-60,0,0,0,127,2,2,0x1eff,0xdfcc,0,0,0)
    sample=bytes((6,4 if loop else 0))+bytes((0x44,))*14+bytes((6,3 if loop else 1))+bytes((0x44,))*14
    return struct.pack('<6I',3,2,24,len(block),24+len(block),len(sample))+block+sample

def synthetic_audio():
    pcm=struct.pack('<8h',100,-100,200,-200,300,-300,400,-400);clips={};native={}
    for name in ('engine','turbo','laser','rock_hit','rock_destroyed','ship_hit','ship_destroyed'):
        render={'sample_rate':48000,'channels':2,'frames':4,'loop_begin_frame':1 if name in ('engine','turbo') else 0,'loop_end_frame':4 if name in ('engine','turbo') else 0}
        clips[name]={'pcm':pcm,**render};native[name]={'pcm_sha256':digest(pcm),'render':render}
    clips['music']={'pcm':pcm,'sample_rate':44100,'channels':1,'frames':8,'loop_begin_frame':2,'loop_end_frame':8}
    native['music']={'pcm_sha256':digest(pcm),'parts':[{'sample_rate':44100}],'loop_begin_frame':2,'loop_end_frame':8}
    return clips,native

class AudioContracts(unittest.TestCase):
    def test_native_class_remap_not_assumed_class_or_local_id(self):
        raw=bank();index=bytearray(0x140);core=bytearray(0x100)
        struct.pack_into('<II',index,0x18,1,0x80);struct.pack_into('<I',index,0x70,0xc0)
        struct.pack_into('<ii',index,0x80,0x40,69);struct.pack_into('<hh',index,0xc8,16,1)
        struct.pack_into('<H',index,0xd0,0);core[0x4d]=1;struct.pack_into('<i',core,0x68,0x40)
        struct.pack_into('<ffiiiiBBHI',core,0x80,0,64,100,512,-1143,508,1,4,0,0)
        result=mapped_sound({'bank':raw,'index':index,'core':core},69,0)
        self.assertEqual(result['bank_sound_id'],0);self.assertEqual(result['render_request_volume'],512)
        self.assertEqual(result['render_request_bend'],-317);self.assertEqual(result['definition']['looped'],1)
        self.assertEqual(result['grains'][0]['tone']['center_note'],-60)
        for cl,slot in ((69,1),(531,0)):
            with self.assertRaises(ValueError):mapped_sound({'bank':raw,'index':index,'core':core},cl,slot)
        with self.assertRaises(ValueError):sound_info(raw,1)
        with self.assertRaises(ValueError):bank_info(b'bad-source')

    def test_music_uses_level_header_track_lba_and_big_endian_vag(self):
        raw=bytearray(8192);toc=bytearray(200);struct.pack_into('<II',toc,13*8,1,1)
        struct.pack_into('<I',raw,2048+0x148+2*4,3)
        raw[6144:6148]=b'VAGp';struct.pack_into('>II',raw,6144+12,16,44100)
        raw[6192:6208]=bytes((12,1))+bytes((0x11,))*14
        source=SimpleNamespace(toc=toc,version=SimpleNamespace(level_table=0),file=io.BytesIO(raw))
        vag,info=music_part(source,13,2)
        self.assertEqual(len(vag),64);self.assertEqual(info['sector'],3);self.assertEqual(info['sample_rate'],44100)
        source.file=io.BytesIO(raw[:6200])
        with self.assertRaises(ValueError):music_part(source,13,2)

    def test_manifest_has_only_combat_roles_preserves_intro_loop_and_aliases(self):
        clips,_=synthetic_audio();rows,payloads,aliases=audio_records(clips)
        self.assertEqual(len(ROLES),13);self.assertNotIn('respawn',aliases);self.assertNotIn('laser_531',aliases)
        cliprows=[s.split() for s in rows if s.startswith('clip ')];self.assertEqual(len(cliprows),13)
        self.assertTrue(all(len(r)==8 for r in cliprows));self.assertEqual(cliprows[0][-2:],['2','8'])
        self.assertEqual(len(payloads),8);self.assertEqual(aliases['engine_531'],aliases['engine_533'])
        self.assertEqual(aliases['turbo_531'],aliases['turbo_533']);self.assertEqual(aliases['laser_532'],aliases['laser_533'])
        self.assertIn('gain music 1',rows);self.assertIn('pitch music 1',rows)
        self.assertTrue(all(row.split()[2]=='1' for row in rows if row.startswith(('gain ','pitch '))))

    def make_pack(self,root):
        base=root/'base';base.mkdir();(base/'provenance.json').write_text(json.dumps({'disc':'pal'}))
        (base/'original-mesh.bin').write_bytes(b'unchanged-geometry')
        (base/'weapons.tsv').write_bytes(b'unchanged-weapons')
        (base/'inventory.json').write_text(json.dumps(file_inventory(base)))
        tool=root/'tool.exe';tool.write_bytes(b'offline-test-renderer')
        fake=SimpleNamespace(region='pal',identity={'region':'pal'},close=lambda:None)
        with patch('audio_assets.Source',return_value=fake),patch('audio_assets.prepare_audio',return_value=synthetic_audio()):
            result=build(base,root/'test.iso',root/'editor',tool,'pal',root/'out',root/'stage')
        return base,root/'out',result

    def test_new_pack_preserves_every_base_payload_and_rejects_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);base,out,result=self.make_pack(root)
            self.assertTrue(result['verified']);self.assertEqual(result['clips'],13)
            for file in ('original-mesh.bin','weapons.tsv'):
                self.assertEqual((base/file).read_bytes(),(out/file).read_bytes())
            self.assertEqual((base/'provenance.json').read_bytes(),(out/'base-flightfx-03.provenance.json').read_bytes())
            with self.assertRaises(ValueError):build(base,root/'x',root/'y',root/'tool.exe','pal',out,root/'stage')
            (out/'audio/engine.pcm.bin').write_bytes(b'corrupt')
            with self.assertRaises(ValueError):verify(out)

    def test_verifier_rejects_bad_format_loop_rate_path_and_missing_semantic(self):
        with tempfile.TemporaryDirectory() as temp:
            _,out,_=self.make_pack(Path(temp));good=(out/'audio.tsv').read_text()
            variants=(good.replace('format 1','format 2'),good+'format 1\n',good+'ambient x\n',
                good.replace('48000 2 4 1 4','48000 2 4 4 4',1),
                good.replace('48000 2 4 1 4','7999 2 4 1 4',1),
                good.replace('audio/engine.pcm.bin','../audio/engine.pcm.bin',1),
                good.replace('audio/engine.pcm.bin',str(out/'audio/engine.pcm.bin'),1),
                good.replace('gain music 1','gain music NaN'),
                good.replace('pitch music 1','pitch music 0'),
                '\n'.join(s for s in good.splitlines() if not s.startswith('clip laser_533 '))+'\n')
            for changed in variants:
                with self.subTest(record=changed[:90]):
                    (out/'audio.tsv').write_text(changed);(out/'inventory.json').write_text(json.dumps(file_inventory(out)))
                    with self.assertRaises(ValueError):verify(out)

    def test_neutral_revision_changes_only_four_metadata_files_and_preserves_frozen_parent(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);_,old,_=self.make_pack(root)
            # Model the historical04 policy in this synthetic fixture only.
            extension=json.loads((old/'audio-extension.json').read_text());extension.pop('mix_policy')
            extension['gain_policy']='Historical music0.4'
            (old/'audio-extension.json').write_text(json.dumps(extension))
            provenance=json.loads((old/'provenance.json').read_text());provenance['scene_audio']=extension
            (old/'provenance.json').write_text(json.dumps(provenance))
            (old/'audio.tsv').write_text((old/'audio.tsv').read_text().replace('gain music 1','gain music 0.4'))
            (old/'inventory.json').write_text(json.dumps(file_inventory(old)))
            before={p.relative_to(old).as_posix():p.read_bytes() for p in old.rglob('*') if p.is_file()}
            new=root/'neutral';result=neutralize(old,new)
            self.assertTrue(result['parent_pack_unchanged']);self.assertTrue(result['all_pcm_and_visual_payloads_byte_identical_to_parent'])
            self.assertEqual(set(result['changed_existing_files']),{'audio.tsv','audio-extension.json','provenance.json','inventory.json'})
            self.assertEqual(before,{p.relative_to(old).as_posix():p.read_bytes() for p in old.rglob('*') if p.is_file()})
            after={p.relative_to(new).as_posix():p.read_bytes() for p in new.rglob('*') if p.is_file()}
            self.assertEqual(set(before),set(after))
            self.assertEqual({name for name in before if before[name]!=after[name]},set(result['changed_existing_files']))
            self.assertEqual((old/'audio/music.pcm.bin').read_bytes(),(new/'audio/music.pcm.bin').read_bytes())
            self.assertIn('gain music 1\n',(new/'audio.tsv').read_text())
            with self.assertRaises(ValueError):neutralize(old,new)
            # A neutral pack may not quietly reintroduce a hidden mix policy.
            for field in ('gain','pitch'):
                text=(new/'audio.tsv').read_text();(new/'audio.tsv').write_text(text.replace(f'{field} music 1',f'{field} music 0.4'))
                (new/'inventory.json').write_text(json.dumps(file_inventory(new)))
                with self.assertRaises(ValueError):verify(new)
                (new/'audio.tsv').write_text(text)
            (new/'inventory.json').write_text(json.dumps(file_inventory(new)))
            with self.assertRaises(ValueError):neutralize(new,root/'again')

    def test_region_mismatch_is_rejected_before_render(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);base=root/'base';base.mkdir();(base/'provenance.json').write_text(json.dumps({'disc':'ntsc'}))
            (base/'inventory.json').write_text(json.dumps(file_inventory(base)))
            fake=SimpleNamespace(region='pal',identity={'region':'pal'},close=lambda:None)
            with patch('audio_assets.Source',return_value=fake),patch('audio_assets.prepare_audio') as render:
                with self.assertRaises(ValueError):build(base,root/'disc',root/'editor',root/'tool','pal',root/'out',root/'stage')
                render.assert_not_called()

@unittest.skipUnless(os.getenv('SPACE_AUDIO_BAKE_TOOL'),'Set SPACE_AUDIO_BAKE_TOOL for original codec CLI contract')
class OfflineCodecContracts(unittest.TestCase):
    def run_tool(self,*args):return subprocess.run([os.environ['SPACE_AUDIO_BAKE_TOOL'],*map(str,args)],capture_output=True,text=True)
    def test_original_adpcm_decode_and_never_overwrite_metadata(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);raw=bytearray(48);raw[:4]=b'VAGp';struct.pack_into('>II',raw,12,16,44100)
            raw+=bytes((12,1))+bytes((0x11,))*14;(root/'in.vag').write_bytes(raw)
            pcm,info=root/'out.pcm',root/'info.json';result=self.run_tool('vag',root/'in.vag',pcm,info)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(struct.unpack('<28h',pcm.read_bytes()),(1,)*28)
            self.assertEqual(json.loads(info.read_text())['sample_rate'],44100)
            info.write_text('preserve-me');result=self.run_tool('vag',root/'in.vag',root/'new.pcm',info)
            self.assertNotEqual(result.returncode,0);self.assertFalse((root/'new.pcm').exists());self.assertEqual(info.read_text(),'preserve-me')
            (root/'bad.vag').write_bytes(raw[:-1]);self.assertNotEqual(self.run_tool('vag',root/'bad.vag',root/'bad.pcm',root/'bad.json').returncode,0)

    def test_original_grain_adsr_spu_loop_and_bad_loop_contract(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/'loop.bank').write_bytes(bank(True));pcm,info=root/'out.pcm',root/'info.json'
            result=self.run_tool('bank',root/'loop.bank',0,1024,0,1,pcm,info)
            self.assertEqual(result.returncode,0,result.stderr);meta=json.loads(info.read_text())
            self.assertEqual(meta['unsupported_grains'],0);self.assertGreater(meta['nonzero_samples'],0)
            self.assertEqual(meta['native_loop_begin_sample'],0);self.assertEqual(meta['native_loop_end_sample'],56)
            self.assertTrue(0<meta['loop_begin_frame']<meta['loop_end_frame']==meta['frames'])
            self.assertEqual(len(pcm.read_bytes()),meta['frames']*4)
            (root/'one.bank').write_bytes(bank(False))
            result=self.run_tool('bank',root/'one.bank',0,1024,0,1,root/'bad.pcm',root/'bad.json')
            self.assertNotEqual(result.returncode,0);self.assertFalse((root/'bad.pcm').exists())

if __name__=='__main__':unittest.main()
