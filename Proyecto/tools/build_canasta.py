"""Write a small low-poly wicker basket. No Blender modules required."""
import math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
vertices=[];faces=[]
def face(points,material):
    ids=[]
    for p in points:vertices.append(p);ids.append(len(vertices))
    faces.append((ids,material))
def band(y,rx,rz,height=.032,thickness=.023):
    for i in range(12):
        a=i*math.tau/12;b=(i+1)*math.tau/12
        outer=[(rx*math.cos(t),h,rz*math.sin(t)) for h in (y,y+height) for t in (a,b)]
        inner=[((rx-thickness)*math.cos(t),h,(rz-thickness)*math.sin(t)) for h in (y,y+height) for t in (a,b)]
        material='MimbreClaro' if i%2 else 'Mimbre'
        for p in ([outer[0],outer[1],outer[3],outer[2]],[inner[1],inner[0],inner[2],inner[3]],
                  [outer[2],outer[3],inner[3],inner[2]],[outer[1],outer[0],inner[0],inner[1]]):face(p,material)
for level in range(5):
    y=.06+level*.132;t=y/.62;band(y,.35+.15*t,.30+.13*t)
band(.605,.50,.43,.04,.035)
for i in range(12):
    angle=i*math.tau/12;tangent=(-math.sin(angle),0,math.cos(angle));radial=(math.cos(angle),0,math.sin(angle))
    low=(.35*radial[0],.015,.30*radial[2]);high=(.50*radial[0],.63,.43*radial[2]);p=[]
    for point in (low,high):
        for a,b in ((-1,-1),(1,-1),(1,1),(-1,1)):
            p.append(tuple(point[k]+a*.014*tangent[k]+b*.010*radial[k] for k in range(3)))
    for ids in ([0,3,2,1],[4,5,6,7],[0,1,5,4],[1,2,6,5],[2,3,7,6],[3,0,4,7]):face([p[j] for j in ids],'MimbreOscuro')
    a=i*math.tau/12;b=(i+1)*math.tau/12
    face([(0,.035,0),(.35*math.cos(a),.035,.30*math.sin(a)),(.35*math.cos(b),.035,.30*math.sin(b))],'Mimbre')
# Arched handle, kept behind the front-side reach of the NPC's hand.
def handle_ring(t):
    center=(.49*math.cos(t),.625+.55*math.sin(t),0)
    length=math.hypot(-.49*math.sin(t),.55*math.cos(t));nx=.55*math.cos(t)/length;ny=.49*math.sin(t)/length
    return [(center[0]+nx*.023*math.cos(a*math.tau/6),center[1]+ny*.023*math.cos(a*math.tau/6),.023*math.sin(a*math.tau/6)) for a in range(6)]
for i in range(18):
    a=handle_ring(i*math.pi/18);b=handle_ring((i+1)*math.pi/18)
    for j in range(6):face([a[j],a[(j+1)%6],b[(j+1)%6],b[j]],'MimbreClaro')
output=['# Canasta de mimbre. Base Y=0; ancho 1.0; material sin texturas externas.','mtllib canasta.mtl','o Canasta','s off']
output+=['v '+' '.join(f'{c:.6f}' for c in p) for p in vertices]
last=None
for ids,material in faces:
    if material!=last:output.append('usemtl '+material);last=material
    output.append('f '+' '.join(map(str,ids)))
(ROOT/'Models/canasta.obj').write_text('\n'.join(output)+'\n',encoding='ascii')
palette={'Mimbre':(.51,.29,.11),'MimbreClaro':(.69,.43,.18),'MimbreOscuro':(.35,.17,.055)}
(ROOT/'Models/canasta.mtl').write_text('\n'.join('newmtl '+n+'\nKd '+' '.join(map(str,c))+'\nKa 0.12 0.08 0.04\nKs 0.05 0.04 0.02\nNs 8\nd 1\nillum 2\n' for n,c in palette.items()),encoding='ascii')
