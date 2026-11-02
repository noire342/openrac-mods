"""Contracts for immutable base packs and original asteroid geometry."""
import json, math, struct, tempfile, unittest
from pathlib import Path
from PIL import Image
from asteroid_assets import build, components, inventory, normalization, source_models, verify
from test_station_assets import fixture_glb

def fixture(directory):
    base = directory / 'base'; base.mkdir()
    entry = {'id':'planet_dummy','vertices':'dummy.vertices.bin','indices':'dummy.indices.bin',
             'rgba':'dummy.rgba.bin','width':1,'height':1,'vertex_count':3,'index_count':3,'vertex_stride':32}
    (base / entry['vertices']).write_bytes(b'original vertex payload')
    (base / entry['indices']).write_bytes(b'original index payload')
    (base / entry['rgba']).write_bytes(bytes((12,34,56,255)))
    (base / 'assets.json').write_text(json.dumps({'format':1,'full_galaxy':True,'region':'pal','assets':[entry]}))
    (base / 'assets.tsv').write_text('planet_dummy dummy.vertices.bin dummy.indices.bin dummy.rgba.bin 1 1 32 10497 10497 OPAQUE\n')
    (base / 'provenance.json').write_text(json.dumps({'disc':'pal','private':True}))
    (base / 'inventory.json').write_text(json.dumps(inventory(base)))
    levels = directory / 'levels'; root = levels / 'level_13'; root.mkdir(parents=True)
    Image.new('RGBA',(3,3),(128,64,32,255)).save(root / 'texture.png')
    classes = {}
    for cid in (212,1412):
        fixture_glb(root / f'moby_{cid}.glb')
        classes[str(cid)] = {'name':'asteroid','mesh':f'moby_{cid}.glb','scale':.14}
    (root / 'manifest.json').write_text(json.dumps({'game':'rac1','version':'pal','level':13,'classes':{'moby':classes}}))
    (root / 'placements.json').write_text(json.dumps({'mobys':[{'class':212}]}))
    return base, levels

class AsteroidExtension(unittest.TestCase):
    def test_shared_normalization_preserves_whole_actor(self):
        def vertex(p): return (p,(0,0,1),(-2,3),(1,1,1,1),-1)
        parts = [([vertex((-4,0,0)),vertex((0,0,0)),vertex((0,1,0))],[0,1,2]),
                 ([vertex((4,0,0)),vertex((0,0,0)),vertex((0,-1,0))],[0,1,2])]
        n = normalization(parts)
        self.assertEqual(n['centre'],[0,0,0]); self.assertEqual(n['radius'],4)
        self.assertGreater(n['divisor'],4)
        self.assertLessEqual(max(math.dist(v[0],n['centre'])/n['divisor'] for p in parts for v in p[0]),1)
        self.assertEqual(len(components(parts)),1) # Material seams join at the original centre.

    def test_disconnected_surface_islands_are_reported_never_discarded(self):
        def vertex(p): return (p,(0,0,1),(0,0),(1,1,1,1),-1)
        vv = [vertex(p) for p in ((0,0,0),(1,0,0),(0,1,0),(5,0,0),(6,0,0),(5,1,0))]
        result = components([(vv,[0,1,2,3,4,5])])
        self.assertEqual([r['triangles'] for r in result],[1,1])
        self.assertEqual(sum(r['triangles'] for r in result),2)

    def test_new_pack_is_append_only_and_preserves_uv_colour_and_indices(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base,levels=fixture(root)
            original={p.name:p.read_bytes() for p in base.iterdir()};out=root/'extended'
            result=build(base,levels,'pal',out)
            self.assertTrue(result['verified']);self.assertEqual(result['total_meshes'],3)
            self.assertEqual({p.name:p.read_bytes() for p in base.iterdir()},original)
            for name,raw in original.items():
                target=out/('base-0.3.3.'+name if name in ('assets.json','assets.tsv','provenance.json','inventory.json') else name)
                self.assertEqual(target.read_bytes(),raw)
            self.assertTrue((out/'assets.tsv').read_bytes().startswith(original['assets.tsv']))
            for name in ('asteroid_0','asteroid_1'):
                vv=list(struct.iter_unpack('<16f',(out/(name+'.vertices.bin')).read_bytes()))
                self.assertEqual(vv[0][6:8],(-2,3));self.assertEqual(vv[0][8:12],(.5,1,1.5,1))
                self.assertEqual(struct.unpack('<3I',(out/(name+'.indices.bin')).read_bytes()),(0,1,2))
                self.assertTrue(all(math.sqrt(sum(x*x for x in v[:3]))<=1 for v in vv))
            # Corruption must be rejected independently of otherwise valid metadata.
            (out/'asteroid_0.indices.bin').write_bytes(struct.pack('<3I',0,1,999))
            with self.assertRaises(ValueError):verify(out)

    def test_region_mixing_and_overwriting_are_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base,levels=fixture(root)
            with self.assertRaises(ValueError):build(base,levels,'ntsc',root/'wrong')
            self.assertFalse((root/'wrong').exists())
            existing=root/'existing';existing.mkdir();(existing/'keep').write_text('original')
            with self.assertRaises(ValueError):build(base,levels,'pal',existing)
            self.assertEqual((existing/'keep').read_text(),'original')

    def test_unexpected_transparent_native_material_fails_before_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base,levels=fixture(root)
            Image.new('RGBA',(3,3),(128,64,32,100)).save(levels/'level_13/texture.png')
            with self.assertRaises(ValueError):build(base,levels,'pal',root/'wrong')
            self.assertFalse((root/'wrong').exists())

if __name__=='__main__':unittest.main()
