"""Native FX decoder/geometry contracts; no game process or installed writes."""
import json,math,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from flight_effects_assets import (NATIVE_COLORS,add,cross,dot,scale,sub,unit,
    logical_palette,gemlik_effects,elf_bytes,native_tables,nozzle_mount,build,extension_data)

class FlightEffectContracts(unittest.TestCase):
    def test_logical_palette_restores_all_csm1_entries_and_ps2_alpha(self):
        raw=bytearray(1024)
        for i in range(256):
            # Four neighbouring eight-entry groups are stored 0,2,1,3.
            block,within=divmod(i,32);group,lane=divmod(within,8)
            physical=block*32+(0,2,1,3)[group]*8+lane
            raw[physical*4:physical*4+4]=bytes((i,255-i,17,128 if i&1 else 32))
        result=logical_palette(raw)
        for i in range(256):self.assertEqual(result[i*4:i*4+4],bytes((i,255-i,17,255 if i&1 else 64)))
        with self.assertRaises(ValueError):logical_palette(raw[:-1])

    def test_native_health_threshold_retains_transparent_entries(self):
        # Native first draw clears logical2..255, later restores onlyj<fill.
        palette=[(j,127,83,255) for j in range(256)];palette[0]=(0,0,0,0);palette[1]=(0,0,0,0)
        for fill in (2,127,253):
            current=palette[:]
            for j in range(2,256):current[j]=(0,0,0,255)
            for j in range(2,fill):current[j]=palette[j]
            self.assertEqual(current[:2],palette[:2]);self.assertEqual(current[fill],(0,0,0,255))
            if fill>2:self.assertEqual(current[fill-1],palette[fill-1])

    def test_effect_bank_offsets_and_dimensions_are_bounded(self):
        index=bytearray(0x100+56*16);core=bytearray(1024+128*128)
        struct.pack_into('<II',index,0x58,56,0x100);struct.pack_into('<I',index,0x68,0)
        struct.pack_into('<4i',index,0x100+49*16,0,1024,128,128)
        core[:1024]=bytes(range(256))*4;core[1024:]=bytes(range(256))*64
        header=bytearray(88);struct.pack_into('<II',header,16,len(header),len(index))
        struct.pack_into('<II',header,80,len(header)+len(index),len(core))
        class Source:
            def level_data(self,level):
                if level!=13:raise AssertionError('Wrong native level')
                return bytes(header+index+core)
        ii,pp,w,h=gemlik_effects(Source())(49)
        self.assertEqual((w,h),(128,128));self.assertEqual(ii,core[1024:]);self.assertEqual(pp,core[:1024])
        struct.pack_into('<4i',index,0x100+49*16,0,len(core),128,128)
        with self.assertRaises(ValueError):gemlik_effects(Source())(49)

    def test_elf_uses_pt_load_mapping_instead_of_address_file_offset(self):
        boot=bytearray(160);boot[:7]=b'\x7fELF\x01\x01\x01';struct.pack_into('<I',boot,28,64)
        struct.pack_into('<HH',boot,42,32,1);struct.pack_into('<8I',boot,64,1,128,0x100000,0,32,32,5,16)
        boot[133:137]=b'TEST';self.assertEqual(elf_bytes(boot,0x100005,4),b'TEST')
        with self.assertRaises(ValueError):elf_bytes(boot,0x10001f,2)
        with self.assertRaises(ValueError):elf_bytes(b'wrong',0,1)

    def test_aperture_follows_tilted_original_ring_not_minus_x_assumption(self):
        normal=unit((-.77,.38,-.5));a=unit(cross(normal,(0,0,1)));b=cross(normal,a)
        centre=(1.3,-.8,.2);points=[];triangles=[];count=32
        for radius in (.25,.4):
            for i in range(count):
                angle=i*math.tau/count
                points.append(add(centre,add(scale(a,radius*math.cos(angle)),scale(b,radius*math.sin(angle)))))
        for i in range(count):
            j=(i+1)%count
            triangles.extend(((i,count+i,count+j),(i,count+j,j)))
        # Make winding outward for this basis.
        if dot(cross(sub(points[triangles[0][1]],points[triangles[0][0]]),sub(points[triangles[0][2]],points[triangles[0][0]])),normal)<0:
            triangles=[(t[0],t[2],t[1]) for t in triangles]
        mount=nozzle_mount(points,triangles)
        for x,y in zip(mount['position'],centre):self.assertAlmostEqual(x,y,places=10)
        for x,y in zip(mount['direction'],normal):self.assertAlmostEqual(x,y,places=10)
        self.assertAlmostEqual(mount['radius'],.25*math.cos(math.pi/count),places=10)
        self.assertAlmostEqual(mount['length'],mount['radius']*4)
        self.assertNotEqual(mount['direction'],[-1,0,0])
        with self.assertRaises(ValueError):nozzle_mount(points,triangles[:3])

    def test_native_gs_tint_keeps_overbright_rgb_and_separate_alpha(self):
        self.assertEqual(NATIVE_COLORS[531],(1.375,.6875,.25,1))
        self.assertEqual(NATIVE_COLORS[533],(0,1,.375,1))
        self.assertEqual(NATIVE_COLORS[531],NATIVE_COLORS[532])

    def test_preexisting_pack_is_never_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);out=root/'existing';out.mkdir();keep=out/'keep';keep.write_bytes(b'immutable')
            with self.assertRaises(ValueError):build(root/'base',root/'levels',root/'disc.iso',root/'editor','pal',out)
            self.assertEqual(keep.read_bytes(),b'immutable')

    def test_mixed_region_rejected_before_source_decode(self):
        class Source:region='pal'
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'provenance.json').write_text(json.dumps({'disc':'ntsc'}))
            with self.assertRaises(ValueError):extension_data(Source(),b'',root,root)

if __name__=='__main__':unittest.main()
