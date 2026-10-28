"""Verifie (port Python de genDecade11_20) qu'aucune etoile n'est dans le solide."""
COLS=20
def run(pos):
    g=[['.']*COLS for _ in range(60)]; stars=[]
    def putH(a,b,r,ch='#'):
        for c in range(a,b+1):
            if 0<=c<COLS and 0<=r<60: g[r][c]=ch
    def putV(a,b,c,ch='#'):
        for r in range(a,b+1):
            if 0<=c<COLS and 0<=r<60: g[r][c]=ch
    def addStar(c,r):
        if len(stars)<3 and r>=0: stars.append((c,r))
    pc=[0,0]
    def col(F,right,segH,floor):
        xo,xi=(18,14) if right else (1,5)
        T=max(F-segH,2)
        if floor: putH(12 if right else 1,18 if right else 7,F)
        else: putH(13 if right else 2,17 if right else 6,F)
        putV(T-2,F-1,xo); putV(T,F-3,xi)
        putH(12 if right else 5,14 if right else 7,T); putH(9,10,T-1)
        addStar((xo+xi)//2,T+2); return T
    def chain(row,c0,d,count,goal):
        cr,cc=row,c0
        for i in range(count):
            cr-=2 if i%3==2 else 1
            if cr<2: cr=2
            w=7 if i==count-1 else 4
            gap=3+i%2; nx=cc+d*gap
            if nx<1 or nx+w>COLS-1: d=-d; nx=cc+d*gap
            nx=max(1,min(nx,COLS-1-w))
            def low(c0):
                return sum(1 for c in range(c0,c0+w) if any(g[rr][c]=='#' for rr in range(cr+1,min(cr+4,60))))>=2
            if low(nx):
                done=False
                for extra in range(4):
                    for sg in range(2):
                        d2=-d if sg else d; n2=cc+d2*(gap+extra)
                        if n2<1 or n2+w>COLS-1 or low(n2): continue
                        nx=n2; d=d2; done=True; break
                    if done: break
            cc=nx
            putH(cc,cc+w-1,cr)
            if i==count//2: addStar(cc+w//2,max(0,cr-2))
        pc[0],pc[1]=cr,cc
    F=58; putH(1,6,F)
    if pos==0: T=col(F,0,7,1); chain(T-1,9,1,2,1)
    elif pos==1: T=col(F,0,7,1); chain(T-1,9,1,3,1)
    elif pos==2: T1=col(F,0,7,1); T2=col(T1,1,7,0); chain(T2-1,9,-1,2,1)
    elif pos==3: T1=col(F,0,7,1);T2=col(T1,1,7,1);T3=col(T2,0,7,1);chain(T3-1,9,1,1,1)
    elif pos==4: T1=col(F,0,7,1);T2=col(T1,1,7,1);chain(T2-1,9,-1,5,1)
    elif pos==5: T1=col(F,0,7,1);chain(T1-1,9,1,3,0);T2=col(pc[0],pc[1]>9,11,1);chain(T2-1,9,1,1,1)
    elif pos==6: T1=col(F,0,7,0);chain(T1-1,9,1,4,0);T2=col(pc[0],pc[1]>9,8,1);chain(T2-1,9,-1,1,1)
    elif pos==7: T1=col(F,0,7,1);T2=col(T1,1,7,0);T3=col(T2,0,7,1);T4=col(T3,1,7,1);chain(T4-1,9,-1,1,1)
    elif pos==8: chain(F-1,4,1,6,0);T=col(pc[0],pc[1]>9,12,0);chain(T-1,9,1,1,1)
    else: T1=col(F,0,7,1);chain(T1-1,9,1,3,0);T2=col(pc[0],pc[1]>9,7,0);chain(T2-1,9,-1,3,0);T3=col(pc[0],pc[1]>9,12,1);chain(T3-1,9,1,1,1)
    return [(c,r,g[r][c]) for c,r in stars if g[r][c]!='.']
for p in range(10): print('niveau',11+p,run(p))
