import sys, itertools
sys.path.insert(0, r'C:\Users\info\Dropbox\Dossier_AntiFaktory\2026\ClaudeRepo\c3-tft-meatboy\tools')
TILE=16; PW=12; PH=14; GRAV=1400.0; JUMP=-450.0; RUN=200.0; MAXF=540.0; DT=0.01

def load(pos):
    src=open(r'C:\Users\info\Dropbox\Dossier_AntiFaktory\2026\ClaudeRepo\c3-tft-meatboy\tools\check_decade2_stars.py',encoding='utf-8').read()
    src=src.replace("    return [(c,r,g[r][c]) for c,r in stars if g[r][c]!='.']","    return g,stars,pc")
    src=src.split("for p in range(10)")[0]
    ns={}; exec(src,ns)
    return ns['run'](pos)

def solid(g,tx,ty):
    if tx<0 or tx>=20: return True
    if ty<0 or ty>=len(g): return False
    return g[ty][tx]=='#'

def collide(g,x,y):
    for ty in range(int(y//TILE), int((y+PH-0.01)//TILE)+1):
        for tx in range(int(x//TILE), int((x+PW-0.01)//TILE)+1):
            if solid(g,tx,ty): return True
    return False

def simulate(g,x,y,vx_dirs,vy0,onground_start=False):
    """vx_dirs: liste de (duree, dir). Renvoie (x,y) d'atterrissage ou None."""
    vy=vy0; t=0.0
    for dur,d in vx_dirs:
        end=t+dur
        while t<end:
            t+=DT
            nx=x+d*RUN*DT
            if collide(g,nx,y): nx=x
            x=nx
            vy=min(vy+GRAV*DT,MAXF)
            ny=y+vy*DT
            if collide(g,x,ny):
                if vy>0:
                    # atterrissage : aligne sur le haut de la tuile
                    ny=int((y+PH)//TILE+1)*TILE-PH if False else y
                    # descendre pixel par pixel
                    yy=y
                    while not collide(g,x,yy+0.5): yy+=0.5
                    return ('land',x,yy)
                else:
                    vy=0; ny=y
            y=ny
            if y>len(g)*TILE: return None
    # continue en chute libre avec la derniere direction
    d=vx_dirs[-1][1]
    for _ in range(400):
        nx=x+d*RUN*DT
        if collide(g,nx,y): nx=x
        x=nx; vy=min(vy+GRAV*DT,MAXF); ny=y+vy*DT
        if collide(g,x,ny):
            if vy>0:
                yy=y
                while not collide(g,x,yy+0.5): yy+=0.5
                return ('land',x,yy)
            vy=0; ny=y
        y=ny
        if y>len(g)*TILE: return None
    return None

def standing(g,x,y):
    return collide(g,x,y+1) and not collide(g,x,y)

def reach(pos,start,goal_tile):
    g,stars,pc=load(pos)
    seen=set(); q=[start]; seen.add((round(start[0]/2),round(start[1])))
    pats=[]
    for d1 in (-1,0,1):
        for d2 in (-1,0,1):
            for t1 in (0.0,0.15,0.3,0.45):
                pats.append([(t1,d1),(0.5,d2)] if t1>0 else [(0.5,d2)])
    n=0
    while q:
        x,y=q.pop(); n+=1
        # but : toucher la tuile du drapeau (rectangle du joueur chevauche)
        gx,gy=goal_tile
        if abs((x+PW/2)-(gx*TILE+8))<14 and abs((y+PH/2)-(gy*TILE+8))<14: return True,n,g
        cand=[]
        # saut
        for p in pats:
            r=simulate(g,x,y,p,JUMP)
            if r: cand.append(r)
        # marche : bords
        for d in (-1,1):
            xx=x
            while True:
                nx=xx+d*2
                if collide(g,nx,y): break
                xx=nx
                if collide(g,xx,y+1) and int(xx)%4==0: cand.append(('land',xx,y))
                if not collide(g,xx,y+1):
                    r=simulate(g,xx,y,[(0.5,d)],0.0)
                    if r: cand.append(r)
                    break
            # point d'arrivee debout le plus loin
            if standing(g,xx,y): cand.append(('land',xx,y))
        for _,nx,ny in cand:
            k=(round(nx/2),round(ny))
            if k not in seen:
                seen.add(k); q.append((nx,ny))
    return False,n,g

if __name__=='__main__':
    pos=int(sys.argv[1]);
    g,stars,pc=load(pos)
    # depart : palier de sortie de la 1ere colonne = (col 6, rangee T) -> debout sur row T-1
    T=51
    start=(6*TILE+2, T*TILE-PH)
    # le drapeau : sur la derniere plateforme, au milieu, une rangee au-dessus
    print('derniere plateforme (row,col):',pc)

def seg(pos,start,goal):
    ok,n,g=reach(pos,start,goal)
    print('atteignable' if ok else 'BLOQUE','etats explores:',n)
