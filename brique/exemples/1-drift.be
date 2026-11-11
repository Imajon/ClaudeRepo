# Drift
# exemple: Drift (vue de dessus, camera qui suit, derapage et traces au sol, rebond sur les bords, chrono, 3 circuits au choix) | mots: course, circuit, voiture, piste, route, kart, moto, derape, drift, virage, chrono
# P3 tenu = accelerer, relache = la vitesse retombe ; G / D = tourner. Derapage dans les virages pris vite (traces au sol).
# Toucher un bord : rebond amorti, perte de vitesse, etincelles. Choix du circuit : G / D, P3 = partir ; a l'arrivee P3 = rejouer, G / D = autre circuit.
# Que des boucles while : sur le firmware d'avant le correctif de be_func.c, l'iterateur d'un for pouvait faire planter le nettoyage de la memoire.
import math
import string

var TR = [
  ["RUBAN", 72, [80, 230, 255], [[75,700],[325,700],[525,675],[650,575],[650,425],[525,338],[325,328],[200,245],[212,120],[412,78],[650,100],[850,162],[950,300],[950,475],[988,650],[1150,725]]],
  ["EPINGLES", 64, [255, 90, 200], [[81,891],[81,270],[113,151],[176,108],[238,151],[270,270],[270,756],[302,875],[364,918],[427,875],[459,756],[459,270],[491,151],[554,108],[616,151],[648,270],[648,756],[680,875],[742,918],[805,875],[837,756],[837,135]]],
  ["SPIRALE", 58, [255, 210, 60], [[112,84],[588,84],[1008,98],[1106,210],[1120,462],[1092,784],[980,882],[812,896],[658,860],[504,896],[210,882],[112,784],[112,420],[182,280],[504,266],[854,280],[938,378],[931,616],[882,707],[784,728],[364,721],[287,630],[301,511],[392,455],[616,462]]]
]
var ACC = 330.0 var VMAX = 290.0 var FRIC = 150.0 var RR = 4.0 var SK = 160 var NP = 24
var CX = [] var CY = [] var LX = [] var LY = [] var RX = [] var RY = [] var PV = [0, 0, 0, 0]
var BB = []                                    # boite de chaque troncon (6 segments) : on ne dessine que ce qui est a l'ecran
var n = 0 var W = 0
var SA = [] var SB = []                        # traces : x * 4096 + y par roue ; SA < 0 = debut d'une trace
var skHead = 0 var skN = 0 var skOn = false
var QX = [] var QY = [] var QVX = [] var QVY = [] var QL = [] var qi = 0
var TRX = [0,0,0,0,0,0,0,0] var TRY = [0,0,0,0,0,0,0,0] var TRA = [0,0,0,0,0,0,0,0] var tf = 0
var tk = 0 var ph = 0 var hits = 0 var cpi = 0 var nxt = 0
var x = 0.0 var y = 0.0 var vx = 0.0 var vy = 0.0 var h = 0.0 var camX = 0.0 var camY = 0.0 var T = 0.0 var shake = 0.0 var cd = 0.0
var best = [0.0, 0.0, 0.0]
var pL = true var pR = true var jLock = true
var PLX = [] var PLY = [] var PX = [0,0,0,0,0,0,0] var PY = [0,0,0,0,0,0,0] var A0 = math.atan2(-9.0, -4.0)
var sq = nil var si = 0 var st = 0.0          # melodie en cours
var SND_WIN = [[523,110],[659,110],[784,110],[1047,260]]
var SND_CLICK = [[1200,12]]

def joue(s) sq = s si = 0 st = 0.0 end
def drC(c, k) return rgb(c[0] * k / 16, c[1] * k / 16, c[2] * k / 16) end
def fmt(t) var cs = int(t * 100 + 0.5) return string.format("%d.%02d", cs / 100, cs % 100) end
def txt(nn, cx, yy, s, c)                      # texte centre avec ombre ; nn = taille de la police d'origine (hauteur 8 * nn)
  var t = nn > 6 ? 6 : nn
  var px = cx - size(s) * 6 * t / 2 var py = yy + (8 * nn - 8 * t) / 2
  texte(px + 1, py + 1, s, 0, t) texte(px, py, s, c, t)
end

def build()                                    # ligne centrale lissee (Catmull-Rom) + bords decales de w/2
  var Tr = TR[tk] var P = Tr[3] var tn = size(P) W = Tr[1]
  CX = [] CY = [] LX = [] LY = [] RX = [] RY = [] BB = []
  var i = 0
  while i < tn - 1
    var i0 = i > 0 ? i - 1 : 0 var i2 = i + 1 var i3 = i + 2 < tn ? i + 2 : tn - 1
    var s = 0
    while s < 6
      var t = s / 6.0 var t2 = t * t var t3 = t2 * t
      var b0 = -0.5 * t3 + t2 - 0.5 * t var b1 = 1.5 * t3 - 2.5 * t2 + 1 var b2 = -1.5 * t3 + 2 * t2 + 0.5 * t var b3 = 0.5 * t3 - 0.5 * t2
      CX.push(b0 * P[i0][0] + b1 * P[i][0] + b2 * P[i2][0] + b3 * P[i3][0])
      CY.push(b0 * P[i0][1] + b1 * P[i][1] + b2 * P[i2][1] + b3 * P[i3][1])
      s += 1
    end
    i += 1
  end
  CX.push(real(P[tn - 1][0])) CY.push(real(P[tn - 1][1])) n = size(CX)
  var k = 0
  while k < n
    var a = k > 0 ? k - 1 : 0 var b = k < n - 1 ? k + 1 : n - 1
    var tx = CX[b] - CX[a] var ty = CY[b] - CY[a] var l = math.sqrt(tx * tx + ty * ty) if l < 0.01 l = 1.0 end
    var nx = -ty / l * W * 0.5 var ny = tx / l * W * 0.5
    LX.push(int(CX[k] + nx)) LY.push(int(CY[k] + ny)) RX.push(int(CX[k] - nx)) RY.push(int(CY[k] - ny))
    k += 1
  end
  var b = 0
  while b < tn - 1
    var x0 = 9999 var y0 = 9999 var x1 = -9999 var y1 = -9999
    i = b * 6
    while i <= b * 6 + 6 && i < n
      x0 = math.min(x0, LX[i], RX[i]) x1 = math.max(x1, LX[i], RX[i]) y0 = math.min(y0, LY[i], RY[i]) y1 = math.max(y1, LY[i], RY[i])
      i += 1
    end
    BB.push(x0 - 3) BB.push(x1 + 3) BB.push(y0 - 3) BB.push(y1 + 3)
    b += 1
  end
  var mx0 = 9999 var my0 = 9999 var mx1 = -9999 var my1 = -9999   # apercu : trace entier reduit
  i = 0
  while i < n
    mx0 = math.min(mx0, LX[i], RX[i]) mx1 = math.max(mx1, LX[i], RX[i]) my0 = math.min(my0, LY[i], RY[i]) my1 = math.max(my1, LY[i], RY[i])
    i += 1
  end
  var sc = math.min(280.0 / (mx1 - mx0), 150.0 / (my1 - my0)) var ox = (320 - int((mx1 - mx0) * sc)) / 2
  PV = [mx0, my0, sc, ox]
end

def reset()                                    # pointeur sur la ligne de depart, cap le long du circuit
  nxt = tk x = CX[1] y = CY[1] vx = 0.0 vy = 0.0 h = math.atan2(CY[3] - CY[0], CX[3] - CX[0])
  camX = x camY = y T = 0.0 hits = 0 cpi = 1 shake = 0.0
  skHead = 0 skN = 0 skOn = false qi = 0
  var i = 0 while i < NP QL[i] = 0.0 i += 1 end
  i = 0 while i < 8 TRX[i] = x TRY[i] = y TRA[i] = h i += 1 end
end

def debut()
  SA.resize(SK) SB.resize(SK)
  var i = 0 while i < NP QX.push(0.0) QY.push(0.0) QVX.push(0.0) QVY.push(0.0) QL.push(0.0) i += 1 end
  var P = [[0,0],[0,15],[4,11],[7,17],[9,16],[6,10],[11,10]]
  i = 0 while i < 7 PLX.push((P[i][0] - 4) * 1.15) PLY.push((P[i][1] - 9) * 1.15) i += 1 end
  build() reset()
end

def spark(px, py, nb, sp)
  var i = 0
  while i < nb
    var a = hasard(0, 627) / 100.0 var v = sp * hasard(30, 99) / 100
    QX[qi] = px QY[qi] = py QVX[qi] = math.cos(a) * v QVY[qi] = math.sin(a) * v QL[qi] = 0.25 + hasard(0, 24) / 100.0
    qi = (qi + 1) % NP
    i += 1
  end
end

def wall(ax, ay, bx, by)                       # collision cercle / segment : repousse le pointeur hors du trait, rebond amorti
  var ex = bx - ax var ey = by - ay var l2 = ex * ex + ey * ey if l2 < 0.01 return end
  var t = ((x - ax) * ex + (y - ay) * ey) / l2 if t < 0 t = 0.0 elif t > 1 t = 1.0 end
  var qx = ax + ex * t var qy = ay + ey * t var dx = x - qx var dy = y - qy var d2 = dx * dx + dy * dy
  if d2 >= RR * RR return end
  var d = math.sqrt(d2) var nx = 0.0 var ny = 0.0
  if d < 0.01 d = math.sqrt(l2) nx = -ey / d ny = ex / d d = 0.0 else nx = dx / d ny = dy / d end
  x += nx * (RR - d) y += ny * (RR - d)
  var vn = vx * nx + vy * ny
  if vn < 0
    vx -= 1.35 * vn * nx vy -= 1.35 * vn * ny vx *= 0.86 vy *= 0.86   # rebond : 35 % de la vitesse d'impact, frottement
    if vn < -50
      hits += 1 shake = math.min(4.0, -vn / 60) spark(qx, qy, 6, 90 - vn * 0.3)
      if vn < -110 joue(SND_CLICK) end
    end
  end
end

def inside()                                   # filet de securite : jamais plus loin que (largeur / 2 - rayon / 2) de la ligne centrale
  var bd = 1e18 var qx = 0.0 var qy = 0.0
  var a = cpi - 4 if a < 0 a = 0 end var b = cpi + 5 if b > n - 2 b = n - 2 end
  var i = a
  while i <= b
    var ax = CX[i] var ay = CY[i] var ex = CX[i + 1] - ax var ey = CY[i + 1] - ay var l2 = ex * ex + ey * ey
    if l2 >= 0.01
      var t = ((x - ax) * ex + (y - ay) * ey) / l2 if t < 0 t = 0.0 elif t > 1 t = 1.0 end
      var px = ax + ex * t var py = ay + ey * t var d = (x - px) * (x - px) + (y - py) * (y - py)
      if d < bd bd = d qx = px qy = py end
    end
    i += 1
  end
  var lim = W * 0.5 - RR * 0.5 var d = math.sqrt(bd)
  if d <= lim || d < 0.01 return end
  var nx = (x - qx) / d var ny = (y - qy) / d
  x = qx + nx * (lim - 0.5) y = qy + ny * (lim - 0.5)
  var vn = vx * nx + vy * ny
  if vn > 0 vx -= 1.35 * vn * nx vy -= 1.35 * vn * ny vx *= 0.86 vy *= 0.86 end
end

def phys(dt, bl, br, bj)
  var sp = math.sqrt(vx * vx + vy * vy) var k = sp / VMAX if k > 1 k = 1.0 end
  var dir = (br ? 1 : 0) - (bl ? 1 : 0)
  h += dir * (3.9 - 1.3 * k) * dt              # on tourne un peu moins vite a pleine vitesse
  var hx = math.cos(h) var hy = math.sin(h)
  var vl = vx * hx + vy * hy var vt = -vx * hy + vy * hx   # vitesse le long du cap / laterale
  if bj vl += ACC * (1 - 0.55 * k) * dt
  else var f = FRIC * dt if math.abs(vl) < f f = math.abs(vl) end vl -= vl > 0 ? f : -f end
  if vl < -30 vl = -30.0 elif vl > VMAX vl = VMAX end
  var ad = 1 - (9.0 - 6.6 * k) * dt if ad < 0 ad = 0.0 end vt *= ad   # adherence : forte a basse vitesse, faible a haute
  vx = hx * vl - hy * vt vy = hy * vl + hx * vt
  var mv = math.sqrt(vx * vx + vy * vy) * dt var ns = int(mv / 2.0) + 1 if ns > 16 ns = 16 end var sdt = dt / ns   # sous-pas de 2 px
  var s = 0
  while s < ns
    x += vx * sdt y += vy * sdt
    var a = cpi - 5 if a < 0 a = 0 end var b = cpi + 6 if b > n - 2 b = n - 2 end   # murs proches seulement
    var i = a
    while i <= b
      var p = LX[i] var q = LX[i + 1]          # test rapide : boite du trait agrandie du rayon
      if (p < q ? x > p - 4 && x < q + 4 : x > q - 4 && x < p + 4)
        p = LY[i] q = LY[i + 1] if (p < q ? y > p - 4 && y < q + 4 : y > q - 4 && y < p + 4) wall(LX[i], LY[i], LX[i + 1], LY[i + 1]) end
      end
      p = RX[i] q = RX[i + 1]
      if (p < q ? x > p - 4 && x < q + 4 : x > q - 4 && x < p + 4)
        p = RY[i] q = RY[i + 1] if (p < q ? y > p - 4 && y < q + 4 : y > q - 4 && y < p + 4) wall(RX[i], RY[i], RX[i + 1], RY[i + 1]) end
      end
      i += 1
    end
    if a == 0 wall(LX[0], LY[0], RX[0], RY[0]) end   # fond de la ligne de depart
    inside()
    s += 1
  end
  var bd = 1e9 var bi = cpi                    # avancement : point de la ligne centrale le plus proche
  var a = cpi - 6 if a < 0 a = 0 end var b = cpi + 12 if b > n - 1 b = n - 1 end
  var i = a
  while i <= b var dx = CX[i] - x var dy = CY[i] - y var d = dx * dx + dy * dy if d < bd bd = d bi = i end i += 1 end
  cpi = bi
  if math.abs(vt) > 42 && sp > 70             # traces de derapage (deux roues)
    var m = int(x - hy * 3) * 4096 + int(y + hx * 3) SA[skHead] = skOn ? m : -1 - m SB[skHead] = int(x + hy * 3) * 4096 + int(y - hx * 3) skOn = true
    skHead = (skHead + 1) % SK if skN < SK skN += 1 end
  else skOn = false end
  if cpi >= n - 2
    var dx = CX[n - 1] - x var dy = CY[n - 1] - y
    if dx * dx + dy * dy < W * W * 0.36
      ph = 3 jLock = true if best[tk] == 0 || T < best[tk] best[tk] = T end joue(SND_WIN)
    end
  end
end

def maj(dt)
  if dt <= 0 dt = 0.016 end if dt > 0.05 dt = 0.05 end
  if sq != nil && temps() >= st                # melodie
    if si < size(sq) son(sq[si][0], sq[si][1]) st = temps() + sq[si][1] / 1000.0 si += 1 else sq = nil end
  end
  var bl = bouton(G) var br = bouton(D) var bj = bouton(P3) var je = appui(P3)
  var eL = bl && !pL var eR = br && !pR pL = bl pR = br
  if !bj jLock = false end
  var j = bj && !jLock
  if ph == 0
    if (eL && !br) || (eR && !bl) tk = (tk + (eR ? 1 : 2)) % 3 build() reset() end
    if je && !jLock ph = 1 cd = 3.0 jLock = true end
  elif ph == 1
    cd -= dt if cd <= 0 ph = 2 T = 0.0 end
  elif ph == 2
    T += dt phys(dt, bl, br, j)
  else
    var f = 1 - math.min(1.0, dt * 2) vx *= f vy *= f x += vx * dt y += vy * dt   # on finit sur sa lancee
    if (eL && !br) || (eR && !bl) nxt = (nxt + (eR ? 1 : 2)) % 3 end   # prochain circuit
    if je && !jLock
      if nxt != tk tk = nxt build() end
      reset() ph = 1 cd = 3.0 jLock = true
    end
  end
  var i = 0
  while i < NP
    if QL[i] > 0 QL[i] -= dt QX[i] += QVX[i] * dt QY[i] += QVY[i] * dt QVX[i] *= 0.9 QVY[i] *= 0.9 end
    i += 1
  end
  tf += 1
  if tf >= 2                                   # trainee du pointeur, une image sur deux
    tf = 0 i = 7
    while i > 0 TRX[i] = TRX[i - 1] TRY[i] = TRY[i - 1] TRA[i] = TRA[i - 1] i -= 1 end
    TRX[0] = x TRY[0] = y TRA[0] = h
  end
  var lx = x + vx * 0.35 var ly = y + vy * 0.35  # camera : en avance sur la vitesse
  var kc = math.min(1.0, dt * 5) camX += (lx - camX) * kc camY += (ly - camY) * kc
  if shake > 0 shake = math.max(0.0, shake - dt * 20) end
end

# ---- dessin ----
def line2(x0, y0, x1, y1, c, c2)               # trait de bord : 2 px + halo
  var dy = y1 - y0 var dx = x1 - x0 if dy < 0 dy = -dy end if dx < 0 dx = -dx end
  if dy > dx
    ligne(x0 - 1, y0, x1 - 1, y1, c2) ligne(x0 + 2, y0, x1 + 2, y1, c2) ligne(x0, y0, x1, y1, c) ligne(x0 + 1, y0, x1 + 1, y1, c)
  else
    ligne(x0, y0 - 1, x1, y1 - 1, c2) ligne(x0, y0 + 2, x1, y1 + 2, c2) ligne(x0, y0, x1, y1, c) ligne(x0, y0 + 1, x1, y1 + 1, c)
  end
end
def pointeur(wx, wy, a, fill, edge, filled)    # pointeur de souris classique, tourne vers le cap
  var c = math.cos(a - A0) var s = math.sin(a - A0) var ox = int(wx) var oy = int(wy)
  var X = PX var Y = PY
  var i = 0 while i < 7 X[i] = ox + int(PLX[i] * c - PLY[i] * s) Y[i] = oy + int(PLX[i] * s + PLY[i] * c) i += 1 end
  if filled
    tri(X[0], Y[0], X[1], Y[1], X[2], Y[2], fill) tri(X[0], Y[0], X[2], Y[2], X[5], Y[5], fill)
    tri(X[2], Y[2], X[3], Y[3], X[4], Y[4], fill) tri(X[2], Y[2], X[4], Y[4], X[5], Y[5], fill) tri(X[0], Y[0], X[5], Y[5], X[6], Y[6], fill)
  end
  i = 0 while i < 7 var k = (i + 1) % 7 ligne(X[i], Y[i], X[k], Y[k], edge) i += 1 end
end
def damier(i, X0, Y0)                          # ligne en damier en travers du circuit au point i
  if CX[i] < X0 - 40 || CX[i] > X0 + 360 || CY[i] < Y0 - 40 || CY[i] > Y0 + 280 return end
  var dx = RX[i] - LX[i] var dy = RY[i] - LY[i] var blanc = rgb(255, 255, 255)
  var k = 0
  while k < 8
    var x0 = int(LX[i] + dx * k / 8.0) var y0 = int(LY[i] + dy * k / 8.0) var x1 = int(LX[i] + dx * (k + 1) / 8.0) var y1 = int(LY[i] + dy * (k + 1) / 8.0)
    ligne(x0, y0, x1, y1, k % 2 ? blanc : 0) ligne(x0, y0 + 1, x1, y1 + 1, k % 2 ? 0 : blanc)
    k += 1
  end
end
def apercu(Tr, c)                              # choix du circuit : trace entier, reduit pour tenir dans l'ecran
  var mx0 = PV[0] var my0 = PV[1] var sc = PV[2] var ox = PV[3] var oy = 58
  var i = 0
  while i < n - 1
    ligne(ox + int((LX[i] - mx0) * sc), oy + int((LY[i] - my0) * sc), ox + int((LX[i + 1] - mx0) * sc), oy + int((LY[i + 1] - my0) * sc), c)
    ligne(ox + int((RX[i] - mx0) * sc), oy + int((RY[i] - my0) * sc), ox + int((RX[i + 1] - mx0) * sc), oy + int((RY[i + 1] - my0) * sc), c)
    i += 1
  end
  disque(ox + int((CX[0] - mx0) * sc), oy + int((CY[0] - my0) * sc), 3, rgb(255, 255, 255))            # depart
  disque(ox + int((CX[n - 1] - mx0) * sc), oy + int((CY[n - 1] - my0) * sc), 3, rgb(255, 60, 60))      # arrivee
  var g = rgb(200, 200, 220)
  txt(3, 160, 14, "<  " + Tr[0] + "  >", c)
  if best[tk] > 0 txt(1, 160, 42, string.format("circuit %d / 3   record %s s", tk + 1, fmt(best[tk])), g)
  else txt(1, 160, 42, string.format("circuit %d / 3   pas encore de record", tk + 1), g) end
  txt(1, 160, 214, "P3 tenu = accelerer   G / D = tourner", g)
  txt(1, 160, 226, "P3 = partir", c)
end

def dessine()
  var Tr = TR[tk] var col = Tr[2]
  var wl = drC(col, 16) var halo = drC(col, 5) var dot = rgb(40, 40, 62) var skid = rgb(46, 44, 60)
  fond(rgb(14, 14, 24))
  if ph == 0 apercu(Tr, wl) return end
  var cxs = camX + (shake > 0 ? math.sin(temps() * 80) * shake : 0)
  var X0 = int(cxs) - 160 var Y0 = int(camY) - 120 var X1 = X0 + 320 var Y1 = Y0 + 240
  camera(X0, Y0)
  var gx0 = X0 / 24 * 24 if gx0 < X0 gx0 += 24 end var gy = Y0 / 24 * 24 if gy < Y0 gy += 24 end   # grille de points (le "bureau")
  while gy < Y1 var gx = gx0 while gx < X1 rect(gx, gy, 1, 1, dot) gx += 24 end gy += 24 end
  var jj = 1
  while jj < skN                               # traces de derapage
    var i = (skHead - skN + jj + SK) % SK var p = (i + SK - 1) % SK
    var a = SA[i]
    if a >= 0
      var b = SA[p] if b < 0 b = -1 - b end
      var ay = a % 4096 var by = b % 4096
      if !((by < Y0 - 2 && ay < Y0 - 2) || (by > Y1 + 2 && ay > Y1 + 2))
        ligne(b / 4096, by, a / 4096, ay, skid) var c = SB[p] var d = SB[i] ligne(c / 4096, c % 4096, d / 4096, d % 4096, skid)
      end
    end
    jj += 1
  end
  var j = 0 var nb = size(BB)
  while j < nb                                 # bords du circuit (seulement les troncons a l'ecran)
    if BB[j + 1] >= X0 && BB[j] <= X1 && BB[j + 3] >= Y0 && BB[j + 2] <= Y1
      var i = j / 4 * 6 var e = i + 6 if e > n - 1 e = n - 1 end
      while i < e line2(LX[i], LY[i], LX[i + 1], LY[i + 1], wl, halo) line2(RX[i], RY[i], RX[i + 1], RY[i + 1], wl, halo) i += 1 end
    end
    j += 4
  end
  line2(LX[0], LY[0], RX[0], RY[0], wl, halo)
  damier(2, X0, Y0) damier(n - 1, X0, Y0)    # depart / arrivee
  var c1 = rgb(255, 240, 160) var c2 = rgb(255, 140, 40)
  var i = 0 while i < NP if QL[i] > 0 rect(int(QX[i]), int(QY[i]), 2, 2, QL[i] > 0.2 ? c1 : c2) end i += 1 end   # etincelles
  i = 7 while i > 0 pointeur(TRX[i], TRY[i], TRA[i], 0, drC(col, 12 - i), false) i -= 2 end   # trainee
  pointeur(x, y, h, rgb(255, 255, 255), 0, true)
  camera(0, 0)
  txt(3, 160, 4, fmt(T), rgb(255, 255, 255))   # chrono, record
  if best[tk] > 0 txt(1, 270, 8, "record " + fmt(best[tk]), wl) end
  txt(1, 40, 8, Tr[0], wl)
  var sp = math.sqrt(vx * vx + vy * vy)        # jauge de vitesse
  cadre(110, 228, 100, 6, halo) rect(111, 229, int(98 * math.min(1.0, sp / VMAX)), 4, wl)
  if ph == 1                                   # compte a rebours
    var k = int(math.ceil(cd)) if k >= 1 && k <= 3 txt(8, 160, 70, str(k), wl) end
  elif ph == 2 && T < 0.7 txt(8, 160, 70, "GO", wl)
  elif ph == 3                                 # arrivee
    rect(60, 60, 200, 120, rgb(24, 22, 40)) cadre(60, 60, 200, 120, wl)
    txt(2, 160, 68, "ARRIVEE", wl)
    txt(5, 160, 90, fmt(T) + " s", rgb(255, 255, 255))
    if T <= best[tk] txt(1, 160, 134, string.format("NOUVEAU RECORD !   %d chocs", hits), wl)
    else txt(1, 160, 134, string.format("record %s s   %d chocs", fmt(best[tk]), hits), wl) end
    txt(1, 160, 156, "<  " + TR[nxt][0] + "  >   P3 = jouer", drC(TR[nxt][2], 16))
  end
end
