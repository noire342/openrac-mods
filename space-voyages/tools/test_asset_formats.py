#!/usr/bin/env python3
"""Focused safeguards for source decoding and scene geometry."""
import math,struct,unittest
from asset_formats import bounded,identity,mul,pif,sphere,transform,trs,values

class Formats(unittest.TestCase):
    def test_accessor_normalized_quaternion(self):
        doc={'accessors':[{'bufferView':0,'componentType':5122,'count':1,'type':'VEC4','normalized':True}],
             'bufferViews':[{'byteOffset':0,'byteLength':8}]}
        data=struct.pack('<hhhh',32767,-32768,0,16384)
        actual=values(doc,data,0)
        self.assertEqual(actual[0][:3],(1.,-1.,0.))
        self.assertAlmostEqual(actual[0][3],.500015259,8)
        # This regression prevents the native int16 ship animation from being
        # interpreted as enormous non-unit quaternion components.
        m=trs({'rotation':actual[0]})
        self.assertAlmostEqual(sum(v*v for v in transform(m,(1,0,0),0)),1.,6)
    def test_accessor_overrun_rejected(self):
        doc={'accessors':[{'bufferView':0,'componentType':5126,'count':2,'type':'VEC3'}],
             'bufferViews':[{'byteOffset':0,'byteLength':12}]}
        with self.assertRaises(ValueError):values(doc,b'\0'*12,0)
    def test_pif_truncation_and_format(self):
        with self.assertRaises(ValueError):pif(b'notpif')
        data=bytearray(0x421);struct.pack_into('<4sIIII',data,0,b'2FIP',0,1,1,0x13)
        self.assertEqual(pif(data).size,(1,1))
        with self.assertRaises(ValueError):pif(data[:0x420])
    def test_transform_order_and_translation(self):
        a=trs({'translation':[2,3,4]});b=trs({'scale':[2,2,2]})
        self.assertEqual(transform(mul(a,b),(1,0,0)),(4,3,4))
        self.assertEqual(transform(identity(),(3,2,1)),(3,2,1))
    def test_sphere_unit_normals_and_winding(self):
        vertices,indices=sphere(16,8)
        self.assertEqual(len(vertices),17*9)
        for v in vertices:self.assertAlmostEqual(sum(x*x for x in v[:3]),1.,8)
        for k in range(0,len(indices),3):
            p,q,r=(vertices[indices[k+j]][:3] for j in range(3));a=[q[c]-p[c] for c in range(3)];b=[r[c]-p[c] for c in range(3)]
            cross=[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
            self.assertGreater(sum(cross[c]*p[c] for c in range(3)),0)
    def test_negative_range(self):
        with self.assertRaises(ValueError):bounded(b'123',-1,2)

if __name__=='__main__':unittest.main()
