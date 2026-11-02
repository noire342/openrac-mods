"""Geometry/immutability contracts for the optional native laser extension."""
import json,math,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from PIL import Image
from asset_formats import digest
from flight_effects_assets import file_inventory
from weapon_assets import aperture_radius,hull2,nacelle_ports_532,build,verify,reticle_data,build_visibility

class ReticleSource:
    region='pal';identity={'region':'pal','verified_by':'synthetic unit fixture'}
    def close(self):pass
    def level_data(self,level):
        if level!=13:raise AssertionError('Reticle must come from original Gemlik bank')
        index=bytearray(0x100+56*16);core=bytearray()
        struct.pack_into('<II',index,0x58,56,0x100);struct.pack_into('<I',index,0x68,0)
        for slot,size in ((17,64),(18,64),(8,32)):
            palette=len(core);core.extend(bytes((20,30,40,32))*256)
            pixels=len(core);core.extend(bytes(size*size))
            struct.pack_into('<4i',index,0x100+slot*16,palette,pixels,size,size)
        header=bytearray(88);struct.pack_into('<II',header,16,len(header),len(index))
        struct.pack_into('<II',header,80,len(header)+len(index),len(core))
        return bytes(header+index+core)

class WeaponAssetContracts(unittest.TestCase):
    def test_octagonal_bore_uses_inscribed_radius_not_bounding_box(self):
        centre=(2,.7,-.2);r=.25
        points=[(2,.7+r*math.cos(i*math.tau/8),-.2+r*math.sin(i*math.tau/8)) for i in range(8)]
        radius,count=aperture_radius(centre,points+points+[centre])
        self.assertEqual(count,8);self.assertAlmostEqual(radius,r*math.cos(math.pi/8),places=12)
        self.assertLess(radius,r)
        with self.assertRaises(ValueError):hull2([(0,0),(1,1),(2,2)])

    def test_port_disk_follows_both_nacelles_and_excludes_blue_nose(self):
        vv=[];ii=[]
        for y in (-.6,0,.6):
            base=len(vv);vv.append((.3,y,.2,1,0,0,0,0))
            for j in range(12):
                a=j*math.tau/12;vv.append((.3,y+.08*math.cos(a),.2+.08*math.sin(a),1,0,0,0,0))
            for j in range(12):ii.extend((base,base+1+j,base+1+(j+1)%12))
        parts=[(vv,ii,None,'OPAQUE')]
        engines=[{'position':[-1,y,.2],'radius':.07} for y in (-.6,.6)]
        with patch('weapon_assets.bake_ship',return_value=(parts,{})),patch('weapon_assets.ship_mounts',return_value=(engines,{})):
            ports,_=nacelle_ports_532(Path('fixture'))
        self.assertEqual(len(ports),2)
        for port,y in zip(ports,(-.6,.6)):
            self.assertAlmostEqual(port['position'][0],.3);self.assertAlmostEqual(port['position'][1],y)
            self.assertAlmostEqual(port['position'][2],.2);self.assertEqual(port['direction'],[1,0,0])
            self.assertEqual(port['surface_triangles'],12);self.assertGreater(abs(port['position'][1]),.5)

    def test_extension_is_append_only_and_catches_corrupt_original_laser(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base=root/'base';base.mkdir();levels=root/'levels';levels.mkdir()
            original={'format':1,'assets':[{'id':'original_ship'}]}
            (base/'assets.json').write_text(json.dumps(original));(base/'assets.tsv').write_bytes(b'original line\n')
            (base/'provenance.json').write_text(json.dumps({'disc':'pal','private':True}))
            (base/'font.rgba.bin').write_bytes(b'original-font');(base/'flight-effects.tsv').write_bytes(b'format 1\n')
            (base/'inventory.json').write_text(json.dumps(file_inventory(base)))
            before={p.name:p.read_bytes() for p in base.iterdir()}
            vertices=[(0,0,0,1,0,0,-2.4,1.9,.5,1,1.5,1),(1,0,0,1,0,0,1,0,1,1,1,1),(1,1,0,1,0,0,0,1,1,1,1,1)]
            image=Image.new('RGBA',(2,2),(255,127,0,255));laser={'wrap_s':10497,'wrap_t':10497,'native_beam_radius':.1}
            records=['format 1','ship 531 0','ship 532 1','ship 533 1','muzzle 532 .3 .6 .2 1 0 0','muzzle 533 .2 .7 .1 1 0 0','tuning 400 200 4','bolt projectile_laser','bolt_scale 532 .2','bolt_scale 533 .1']
            muzzles={str(cl):{'muzzles':[{'position':[.3,.6,.2]}],'scaled_beam_radius':.02,'minimum_port_radius':.03,'bolt_scale':.2} for cl in (532,533)}
            with patch('weapon_assets.laser_source',return_value=(vertices,[0,1,2],image,laser)),patch('weapon_assets.muzzle_data',return_value=(records,muzzles)):
                result=build(base,levels,'pal',root/'out');out=root/'out'
                self.assertTrue(result['verified']);self.assertEqual(result['meshes'],2)
                self.assertEqual(before,{p.name:p.read_bytes() for p in base.iterdir()})
                self.assertEqual((out/'assets.tsv').read_bytes()[:len(before['assets.tsv'])],before['assets.tsv'])
                self.assertEqual((out/'font.rgba.bin').read_bytes(),before['font.rgba.bin'])
                self.assertEqual((out/'flight-effects.tsv').read_bytes(),before['flight-effects.tsv'])
                self.assertEqual(json.loads((out/'assets.json').read_text())['assets'][:-1],original['assets'])
                (out/'projectile_laser.indices.bin').write_bytes(b'corrupt')
                with self.assertRaises(ValueError):verify(out)

    def test_existing_pack_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);out=p/'exists';out.mkdir();(out/'keep').write_bytes(b'keep')
            with self.assertRaises(ValueError):build(p/'base',p/'levels','pal',out)
            self.assertEqual((out/'keep').read_bytes(),b'keep')

    def test_native_reticle_uses_exact_slots_and_preserves_rgb_ps2_alpha(self):
        payloads,records,provenance=reticle_data(ReticleSource())
        self.assertEqual(sum(map(len,payloads.values())),36864)
        self.assertEqual([provenance[k]['effect_slot'] for k in ('ring','marks','center')],[17,18,8])
        for name,raw in payloads.items():
            self.assertEqual(raw[:4],bytes((20,30,40,64)));self.assertEqual(set(raw[3::4]),{64})
        self.assertEqual(provenance['ring']['native_uv'],[1/64,1/64,63/64,63/64])
        self.assertEqual(provenance['center']['native_draw_size'],[10,10])
        self.assertTrue(records[0].startswith('reticle ring '))

    def test_visibility_extension_changes_only_declared_visual_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base=root/'base';base.mkdir()
            (base/'assets.json').write_text(json.dumps({'assets':[{'id':'projectile_laser'}]}))
            (base/'assets.tsv').write_bytes(b'unchanged mesh rows\n')
            (base/'projectile_laser.vertices.bin').write_bytes(b'original geometry')
            (base/'flight-effects.tsv').write_bytes(b'format 1\noriginal health and flame\n')
            (base/'weapons.tsv').write_bytes(b'format 1\ntuning 400 200 4\nbolt projectile_laser\n')
            (base/'provenance.json').write_text(json.dumps({'disc':'pal'}))
            (base/'inventory.json').write_text(json.dumps(file_inventory(base)))
            before={p.name:p.read_bytes() for p in base.iterdir()}
            with patch('weapon_assets.verify_weapons',return_value={'region':'pal'}),patch('weapon_assets.Source',return_value=ReticleSource()):
                result=build_visibility(base,root/'disc.iso',root/'editor','pal',root/'out')
                self.assertTrue(result['verified']);self.assertEqual(result['added_raw_bytes'],36864)
                self.assertEqual(before,{p.name:p.read_bytes() for p in base.iterdir()})
                out=root/'out';weapon=(out/'weapons.tsv').read_bytes()
                self.assertTrue(weapon.startswith(before['weapons.tsv']))
                self.assertIn(b'bolt_growth 532 3 6\n',weapon);self.assertIn(b'bolt_growth 533 6 6\n',weapon)
                self.assertEqual((out/'assets.tsv').read_bytes(),before['assets.tsv'])
                self.assertEqual((out/'projectile_laser.vertices.bin').read_bytes(),before['projectile_laser.vertices.bin'])
                self.assertEqual((out/'flight-effects.tsv').read_bytes(),before['flight-effects.tsv'])
                self.assertEqual((out/'base-flightfx-02.weapons.tsv').read_bytes(),before['weapons.tsv'])
                (out/'reticle-center.rgba.bin').write_bytes(b'corrupt')
                with self.assertRaises(ValueError):verify(out)

    def test_visibility_rejects_region_mix_before_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base=root/'base';base.mkdir()
            for name in ('weapons.tsv','provenance.json'):(base/name).write_bytes(b'unchanged')
            (base/'inventory.json').write_text(json.dumps(file_inventory(base)))
            with patch('weapon_assets.verify_weapons',return_value={'region':'ntsc'}),patch('weapon_assets.Source',return_value=ReticleSource()):
                with self.assertRaises(ValueError):build_visibility(base,root/'disc.iso',root/'editor','pal',root/'bad')
            self.assertFalse((root/'bad').exists())

if __name__=='__main__':unittest.main()
