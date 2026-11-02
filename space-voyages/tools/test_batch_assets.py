"""Shader/atlas sampling regressions and lossless geometry contracts."""
import random,struct,unittest
from PIL import Image
from batch_assets import CLAMP,REPEAT,atlas_sample,bilinear,check_bilinear,chunks,expand_vertices,make_atlas,pack_tiles,padded_tile

def pattern(w,h):
    image=Image.new('RGBA',(w,h));image.putdata([((x*79+y*23)%256,(y*89+x*17)%256,(x*47+y*61)%256,(x*31+y*83)%256) for y in range(h) for x in range(w)])
    return image

class AtlasBatching(unittest.TestCase):
    def test_repeat_and_clamp_bilinear_seams_all_axes(self):
        image=pattern(7,5);tile={'key':'source','width':7,'height':5,'rgba':image.tobytes()}
        for s in (REPEAT,CLAMP):
            for t in (REPEAT,CLAMP):
                atlas,rects,_=make_atlas([tile],s,t);rect=rects['source']
                self.assertLess(check_bilinear(image,atlas,rect,s,t),1e-7)
                rng=random.Random(571)
                for _ in range(150):
                    u,v=(rng.uniform(-4,4) for _ in range(2))
                    expected=bilinear(image,u,v,s,t);actual=atlas_sample(atlas,rect,u,v,s,t)
                    for a,b in zip(expected,actual):self.assertAlmostEqual(a,b,9)
                self.assertEqual(struct.unpack('<4f',struct.pack('<4f',*rect)),rect)
    def test_wrap_happens_after_interpolation(self):
        image=pattern(4,4);atlas,rects,_=make_atlas([{'key':'a','width':4,'height':4,'rgba':image.tobytes()}],REPEAT,REPEAT)
        # A UV seam crossing: interpolate .9 -> 1.1 to 1.0, then wrap to0.
        correct=atlas_sample(atlas,rects['a'],(.9+1.1)/2,.4,REPEAT,REPEAT)
        naive=atlas_sample(atlas,rects['a'],(.9+.1)/2,.4,REPEAT,REPEAT)
        self.assertNotEqual(correct,naive)
        for a,b in zip(correct,bilinear(image,0,.4,REPEAT,REPEAT)):self.assertAlmostEqual(a,b,10)
    def test_two_pixel_gutters_include_corners(self):
        image=pattern(3,4)
        for s in (REPEAT,CLAMP):
            for t in (REPEAT,CLAMP):
                tile=padded_tile(image,s,t)
                for y in range(tile.height):
                    for x in range(tile.width):
                        xx=(x-2)%3 if s==REPEAT else min(2,max(0,x-2))
                        yy=(y-2)%4 if t==REPEAT else min(3,max(0,y-2))
                        self.assertEqual(tile.getpixel((x,y)),image.getpixel((xx,yy)))
    def test_variable_size_packing_no_overlap(self):
        tiles=[{'key':str(i),'width':w,'height':h} for i,(w,h) in enumerate(((32,32),(128,128),(64,32),(32,128),(256,256),(128,64),(32,32)))]
        width,height,positions=pack_tiles(tiles)
        self.assertEqual(width&(width-1),0);self.assertEqual(height&(height-1),0)
        rects=[]
        for tile in tiles:
            x,y=positions[tile['key']];w,h=tile['width']+4,tile['height']+4
            self.assertLessEqual(x+w,width);self.assertLessEqual(y+h,height)
            for a,b,c,d in rects:self.assertTrue(x+w<=a or a+c<=x or y+h<=b or b+d<=y)
            rects.append((x,y,w,h))
        self.assertEqual(pack_tiles(list(reversed(tiles))),(width,height,positions))
    def test_vertex_bytes_unchanged_and_rect_constant(self):
        source=b''.join(struct.pack('<12f',*(float(i+k)/7 for k in range(12))) for i in range(3));rect=(.25,.125,.5,.75)
        expanded=expand_vertices(source,rect);self.assertEqual(len(expanded),3*64)
        for i in range(3):
            self.assertEqual(expanded[i*64:i*64+48],source[i*48:(i+1)*48])
            self.assertEqual(expanded[i*64+48:(i+1)*64],struct.pack('<4f',*rect))
        with self.assertRaises(ValueError):expand_vertices(source+b'bad',rect)
    def test_chunks_preserve_whole_material_order(self):
        rows=[{'id':str(i),'vertex_count':n} for i,n in enumerate((3,2,4,1))]
        result=chunks(rows,5)
        self.assertEqual([[r['id'] for r in chunk] for chunk in result],[['0','1'],['2','3']])
        with self.assertRaises(ValueError):chunks([{'vertex_count':6}],5)

if __name__=='__main__':unittest.main()
