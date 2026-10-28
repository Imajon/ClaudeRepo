import re, math, struct
src = open('c3-tft-meatboy.kicad_pcb', encoding='utf-8').read()
T = float(re.search(r'\(thickness ([\d.]+)\)', src).group(1))
arcs = []
for m in re.finditer(r'\(gr_arc\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(mid ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)(.*?)\(layer "Edge\.Cuts"\)', src, re.S):
    v = list(map(float, m.groups()[:6])); arcs.append(((v[0],v[1]),(v[2],v[3]),(v[4],v[5])))
def arcpts(a, b, c, n=64):
    ax,ay=a; bx,by=b; cx,cy=c
    d = 2*(ax*(by-cy)+bx*(cy-ay)+cx*(ay-by))
    ux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/d
    uy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/d
    r = math.hypot(ax-ux, ay-uy)
    t0=math.atan2(ay-uy,ax-ux); t1=math.atan2(by-uy,bx-ux); t2=math.atan2(cy-uy,cx-ux)
    tw=lambda t:(t)%(2*math.pi)
    ccw = tw(t1-t0) < tw(t2-t0)
    span = tw(t2-t0) if ccw else -tw(t0-t2)
    return [(ux+r*math.cos(t0+span*i/n), uy+r*math.sin(t0+span*i/n)) for i in range(n+1)]
close=lambda p,q: math.hypot(p[0]-q[0],p[1]-q[1])<0.01
lines=[]
for m in re.finditer(r'\(gr_line\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)(.*?)\(layer "Edge\.Cuts"\)', src, re.S):
    v = list(map(float, m.groups()[:4])); lines.append([(v[0],v[1]),(v[2],v[3])])
segs=[arcpts(*a) for a in arcs]+lines
poly=segs.pop(0)
while segs:
    for i,s in enumerate(segs):
        if close(s[0],poly[-1]): poly+=s[1:]; segs.pop(i); break
        if close(s[-1],poly[-1]): poly+=s[::-1][1:]; segs.pop(i); break
    else: raise SystemExit('contour non fermé')
assert close(poly[0],poly[-1]); poly=poly[:-1]
# Y KiCad vers le bas -> inverser
poly=[(x,-y) for x,y in poly]
area=sum(poly[i][0]*poly[(i+1)%len(poly)][1]-poly[(i+1)%len(poly)][0]*poly[i][1] for i in range(len(poly)))/2
if area<0: poly=poly[::-1]
xs=[p[0] for p in poly]; ys=[p[1] for p in poly]
mx,my=min(xs),min(ys); poly=[(x-mx,y-my) for x,y in poly]
# ear clipping
def cross(o,a,b): return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
def inside(p,a,b,c): return cross(a,b,p)>=0 and cross(b,c,p)>=0 and cross(c,a,p)>=0
idx=list(range(len(poly))); tris=[]
while len(idx)>3:
    for k in range(len(idx)):
        i0,i1,i2=idx[k-1],idx[k],idx[(k+1)%len(idx)]
        a,b,c=poly[i0],poly[i1],poly[i2]
        if cross(a,b,c)<=1e-12: continue
        if any(inside(poly[j],a,b,c) for j in idx if j not in (i0,i1,i2)): continue
        tris.append((i0,i1,i2)); idx.pop(k); break
    else: raise SystemExit('triangulation échouée')
tris.append(tuple(idx))
V=lambda i,z:(poly[i][0],poly[i][1],z)
faces=[]
for a,b,c in tris:
    faces.append((V(a,T),V(b,T),V(c,T))); faces.append((V(a,0),V(c,0),V(b,0)))
n=len(poly)
for i in range(n):
    j=(i+1)%n
    faces.append((V(i,0),V(j,0),V(j,T))); faces.append((V(i,0),V(j,T),V(i,T)))
def normal(f):
    (a,b,c)=f; u=[b[k]-a[k] for k in range(3)]; v=[c[k]-a[k] for k in range(3)]
    nx,ny,nz=u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]; l=math.sqrt(nx*nx+ny*ny+nz*nz) or 1
    return (nx/l,ny/l,nz/l)
with open('c3-tft-meatboy-contour.stl','wb') as f:
    f.write(b'c3-tft-meatboy Edge.Cuts'.ljust(80,b' ')); f.write(struct.pack('<I',len(faces)))
    for fc in faces: f.write(struct.pack('<12fH',*normal(fc),*fc[0],*fc[1],*fc[2],0))
print(len(faces),'faces, %.2f x %.2f x %.2f mm'%(max(xs)-mx,max(ys)-my,T))
