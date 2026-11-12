# Drift
# exemple: Drift (vue de dessus, camera qui suit, derapage et traces au sol, rebond sur les bords, chrono, 3 circuits au choix) | mots: course, circuit, voiture, piste, route, kart, moto, derape, drift, virage, chrono
# P3 tenu = accelerer, relache = la vitesse retombe ; G / D = tourner. Derapage dans les virages pris vite (traces au sol).
# Toucher un bord : rebond amorti, perte de vitesse. Choix du circuit : G / D, P3 = partir ; a l'arrivee P3 = rejouer, G / D = autre circuit.
# for pour preparer (debut), while dans maj() et dessine() : un for y fabrique un iterateur a chaque image.
import math
import string

var TR = [
  ["RUBAN", 72, [80, 230, 255], [75,700,325,700,525,675,650,575,650,425,525,338,325,328,200,245,212,120,412,78,650,100,850,162,950,300,950,475,988,650,1150,725]],
  ["EPINGLES", 64, [255, 90, 200], [81,891,81,270,113,151,176,108,238,151,270,270,270,756,302,875,364,918,427,875,459,756,459,270,491,151,554,108,616,151,648,270,648,756,680,875,742,918,805,875,837,756,837,135]],
  ["SPIRALE", 58, [255, 210, 60], [112,84,588,84,1008,98,1106,210,1120,462,1092,784,980,882,812,896,658,860,504,896,210,882,112,784,112,420,182,280,504,266,854,280,938,378,931,616,882,707,784,728,364,721,287,630,301,511,392,455,616,462]]
]
var ACC = 330.0 var VMAX = 290.0 var FRIC = 150.0 var RR = 4.0 var SK = 160
var CX = [] var CY = [] var LX = [] var LY = [] var RX = [] var RY = [] var PV = []
var BB = []                                    # boite de chaque troncon (6 segments) : on ne dessine que ce qui est a l'ecran
var n = 0 var W = 0
var SA = [] var SB = []                        # traces : x * 4096 + y par roue ; SA < 0 = debut d'une trace
var skHead = 0 var skN = 0 var skOn = false
var tk = 0 var ph = 0 var hits = 0 var cpi = 0 var nxt = 0
var x = 0.0 var y = 0.0 var vx = 0.0 var vy = 0.0 var h = 0.0 var camX = 0.0 var camY = 0.0 var T = 0.0 var cd = 0.0
var best = [0.0, 0.0, 0.0]

def coul(c) return rgb(c[0], c[1], c[2]) end
def fmt(t) var cs = int(t * 100 + 0.5) return string.format("%d.%02d", cs / 100, cs % 100) end
def txt(t, cx, yy, s, c) texte(cx - size(s) * 3 * t, yy, s, c, t) end   # texte centre en cx, taille t

def build()                                    # ligne centrale lissee (Catmull-Rom) + bords decales de W / 2
  var P = TR[tk][3] var tn = size(P) / 2 W = TR[tk][1]   # P = x, y, x, y...
  CX = [] CY = [] LX = [] LY = [] RX = [] RY = [] BB = []
  for i: 0 .. tn - 2
    var i0 = i > 0 ? i - 1 : 0 var i2 = i + 1 var i3 = i + 2 < tn ? i + 2 : tn - 1
    for s: 0 .. 5
      var t = s / 6.0 var t2 = t * t var t3 = t2 * t
      var b0 = -0.5 * t3 + t2 - 0.5 * t var b1 = 1.5 * t3 - 2.5 * t2 + 1 var b2 = -1.5 * t3 + 2 * t2 + 0.5 * t var b3 = 0.5 * t3 - 0.5 * t2
      CX.push(b0 * P[2 * i0] + b1 * P[2 * i] + b2 * P[2 * i2] + b3 * P[2 * i3])
      CY.push(b0 * P[2 * i0 + 1] + b1 * P[2 * i + 1] + b2 * P[2 * i2 + 1] + b3 * P[2 * i3 + 1])
    end
  end
  CX.push(real(P[2 * tn - 2])) CY.push(real(P[2 * tn - 1])) n = size(CX)
  for k: 0 .. n - 1
    var a = k > 0 ? k - 1 : 0 var b = k < n - 1 ? k + 1 : n - 1
    var tx = CX[b] - CX[a] var ty = CY[b] - CY[a] var l = math.sqrt(tx * tx + ty * ty) if l < 0.01 l = 1.0 end
    var nx = -ty / l * W * 0.5 var ny = tx / l * W * 0.5
    LX.push(int(CX[k] + nx)) LY.push(int(CY[k] + ny)) RX.push(int(CX[k] - nx)) RY.push(int(CY[k] - ny))
  end
  var mx0 = 9999 var my0 = 9999 var mx1 = -9999 var my1 = -9999   # apercu : trace entier reduit
  for b: 0 .. tn - 2
    var x0 = 9999 var y0 = 9999 var x1 = -9999 var y1 = -9999
    for i: b * 6 .. b * 6 + 6
      x0 = math.min(x0, LX[i], RX[i]) x1 = math.max(x1, LX[i], RX[i]) y0 = math.min(y0, LY[i], RY[i]) y1 = math.max(y1, LY[i], RY[i])
    end
    BB.push(x0 - 3) BB.push(x1 + 3) BB.push(y0 - 3) BB.push(y1 + 3)
    mx0 = math.min(mx0, x0) mx1 = math.max(mx1, x1) my0 = math.min(my0, y0) my1 = math.max(my1, y1)
  end
  var sc = math.min(280.0 / (mx1 - mx0), 150.0 / (my1 - my0))
  PV = [mx0, my0, sc, (320 - int((mx1 - mx0) * sc)) / 2]
end

def reset()                                    # sur la ligne de depart, cap le long du circuit
  nxt = tk x = CX[1] y = CY[1] vx = 0.0 vy = 0.0 h = math.atan2(CY[3] - CY[0], CX[3] - CX[0])
  camX = x camY = y T = 0.0 hits = 0 cpi = 1 skN = 0 skOn = false
end

def debut() SA.resize(SK) SB.resize(SK) build() reset() end

def rebond(nx, ny)                             # bord de normale (nx, ny) : on garde 35 % de la vitesse d'impact, frottement
  var vn = vx * nx + vy * ny
  if vn < 0 vx -= 1.35 * vn * nx vy -= 1.35 * vn * ny vx *= 0.86 vy *= 0.86 end
  return vn
end

def wall(ax, ay, bx, by)                       # collision cercle / segment : repousse hors du trait
  var ex = bx - ax var ey = by - ay var l2 = ex * ex + ey * ey if l2 < 0.01 return end
  var t = ((x - ax) * ex + (y - ay) * ey) / l2 if t < 0 t = 0.0 elif t > 1 t = 1.0 end
  var dx = x - ax - ex * t var dy = y - ay - ey * t var d2 = dx * dx + dy * dy
  if d2 >= RR * RR return end
  var d = math.sqrt(d2) var nx = 0.0 var ny = 0.0
  if d < 0.01 d = math.sqrt(l2) nx = -ey / d ny = ex / d d = 0.0 else nx = dx / d ny = dy / d end
  x += nx * (RR - d) y += ny * (RR - d)
  var vn = rebond(nx, ny)
  if vn < -50 hits += 1 end
  if vn < -110 son(1200, 12) end
end

def inside()                                   # filet de securite : jamais plus loin que W / 2 - RR / 2 de la ligne centrale
  var bd = 1e18 var qx = 0.0 var qy = 0.0
  var i = math.max(0, cpi - 4) var b = math.min(n - 2, cpi + 5)
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
  rebond(-nx, -ny)
end

def phys(dt, bl, br, bj)
  var sp = math.sqrt(vx * vx + vy * vy) var k = math.min(1.0, sp / VMAX)
  h += ((br ? 1 : 0) - (bl ? 1 : 0)) * (3.9 - 1.3 * k) * dt   # on tourne un peu moins vite a pleine vitesse
  var hx = math.cos(h) var hy = math.sin(h)
  var vl = vx * hx + vy * hy var vt = -vx * hy + vy * hx   # vitesse le long du cap / laterale
  if bj vl += ACC * (1 - 0.55 * k) * dt
  else var f = math.min(FRIC * dt, math.abs(vl)) vl -= vl > 0 ? f : -f end
  if vl < -30 vl = -30.0 elif vl > VMAX vl = VMAX end
  vt *= math.max(0.0, 1 - (9.0 - 6.6 * k) * dt)   # adherence : forte a basse vitesse, faible a haute
  vx = hx * vl - hy * vt vy = hy * vl + hx * vt
  var ns = math.min(16, int(math.sqrt(vx * vx + vy * vy) * dt / 2.0) + 1) var sdt = dt / ns   # sous-pas de 2 px
  var a = math.max(0, cpi - 5) var b = math.min(n - 2, cpi + 6)   # murs proches seulement
  var s = 0
  while s < ns
    x += vx * sdt y += vy * sdt
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
  var i = math.max(0, cpi - 6) var e = math.min(n - 1, cpi + 12)
  while i <= e var dx = CX[i] - x var dy = CY[i] - y var d = dx * dx + dy * dy if d < bd bd = d bi = i end i += 1 end
  cpi = bi
  if math.abs(vt) > 42 && sp > 70             # traces de derapage (deux roues)
    var m = int(x - hy * 3) * 4096 + int(y + hx * 3) SA[skHead] = skOn ? m : -1 - m SB[skHead] = int(x + hy * 3) * 4096 + int(y - hx * 3) skOn = true
    skHead = (skHead + 1) % SK if skN < SK skN += 1 end
  else skOn = false end
  var dx = CX[n - 1] - x var dy = CY[n - 1] - y
  if cpi >= n - 2 && dx * dx + dy * dy < W * W * 0.36   # arrivee
    ph = 3 if best[tk] == 0 || T < best[tk] best[tk] = T end son(1047, 260)
  end
end

def maj(dt)
  dt = math.min(0.05, dt > 0 ? dt : 0.016)
  var bl = bouton(G) var br = bouton(D)
  var ch = appui(D) && !bl ? 1 : (appui(G) && !br ? 2 : 0)   # circuit suivant (+1) / precedent (+2 modulo 3)
  if ph == 0
    if ch > 0 tk = (tk + ch) % 3 build() reset() end
    if appui(P3) ph = 1 cd = 3.0 end
  elif ph == 1
    cd -= dt if cd <= 0 ph = 2 T = 0.0 end
  elif ph == 2
    T += dt phys(dt, bl, br, bouton(P3))
  else
    var f = 1 - math.min(1.0, dt * 2) vx *= f vy *= f x += vx * dt y += vy * dt   # on finit sur sa lancee
    nxt = (nxt + ch) % 3                       # prochain circuit
    if appui(P3)
      if nxt != tk tk = nxt build() end
      reset() ph = 1 cd = 3.0
    end
  end
  var kc = math.min(1.0, dt * 5)               # camera : en avance sur la vitesse
  camX += (x + vx * 0.35 - camX) * kc camY += (y + vy * 0.35 - camY) * kc
end

# ---- dessin ----
def apercu(Tr, c, g)                           # choix du circuit : trace entier, reduit pour tenir dans l'ecran
  var mx0 = PV[0] var my0 = PV[1] var sc = PV[2] var ox = PV[3] var oy = 58
  var i = 0
  while i < n - 1
    ligne(ox + int((LX[i] - mx0) * sc), oy + int((LY[i] - my0) * sc), ox + int((LX[i + 1] - mx0) * sc), oy + int((LY[i + 1] - my0) * sc), c)
    ligne(ox + int((RX[i] - mx0) * sc), oy + int((RY[i] - my0) * sc), ox + int((RX[i + 1] - mx0) * sc), oy + int((RY[i + 1] - my0) * sc), c)
    i += 1
  end
  disque(ox + int((CX[0] - mx0) * sc), oy + int((CY[0] - my0) * sc), 3, rgb(255, 255, 255))            # depart
  disque(ox + int((CX[n - 1] - mx0) * sc), oy + int((CY[n - 1] - my0) * sc), 3, rgb(255, 60, 60))      # arrivee
  txt(3, 160, 14, "<  " + Tr[0] + "  >", c)
  txt(1, 160, 42, string.format("circuit %d / 3   ", tk + 1) + (best[tk] > 0 ? "record " + fmt(best[tk]) + " s" : "pas encore de record"), g)
  txt(1, 160, 214, "P3 tenu = accelerer   G / D = tourner", g)
  txt(1, 160, 226, "P3 = partir", c)
end

def dessine()
  var Tr = TR[tk] var wl = coul(Tr[2]) var gris = rgb(200, 200, 220) var blanc = rgb(255, 255, 255)
  fond(rgb(14, 14, 24))
  if ph == 0 apercu(Tr, wl, gris) return end
  var X0 = int(camX) - 160 var Y0 = int(camY) - 120 var X1 = X0 + 320 var Y1 = Y0 + 240
  camera(X0, Y0)
  var dot = rgb(40, 40, 62) var skid = rgb(46, 44, 60)
  var gx0 = X0 / 24 * 24 if gx0 < X0 gx0 += 24 end var gy = Y0 / 24 * 24 if gy < Y0 gy += 24 end   # grille de points au sol
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
      var i = j / 4 * 6 var e = i + 6
      while i < e ligne(LX[i], LY[i], LX[i + 1], LY[i + 1], wl) ligne(RX[i], RY[i], RX[i + 1], RY[i + 1], wl) i += 1 end
    end
    j += 4
  end
  ligne(LX[0], LY[0], RX[0], RY[0], wl)       # fond de la ligne de depart
  ligne(LX[2], LY[2], RX[2], RY[2], blanc) ligne(LX[n - 1], LY[n - 1], RX[n - 1], RY[n - 1], blanc)   # depart / arrivee
  var c = math.cos(h) var s = math.sin(h)      # voiture : triangle pointe vers le cap
  tri(int(x + c * 8), int(y + s * 8), int(x - c * 5 - s * 5), int(y - s * 5 + c * 5), int(x - c * 5 + s * 5), int(y - s * 5 - c * 5), blanc)
  camera(0, 0)
  txt(3, 160, 4, fmt(T), blanc)                # chrono, record
  if best[tk] > 0 txt(1, 270, 8, "record " + fmt(best[tk]), wl) end
  txt(1, 40, 8, Tr[0], wl)
  cadre(110, 228, 100, 6, gris) rect(111, 229, int(98 * math.min(1.0, math.sqrt(vx * vx + vy * vy) / VMAX)), 4, wl)   # jauge de vitesse
  if ph == 1 txt(6, 160, 78, str(int(math.ceil(cd))), wl)   # compte a rebours
  elif ph == 2 && T < 0.7 txt(6, 160, 78, "GO", wl)
  elif ph == 3                                 # arrivee
    rect(60, 60, 200, 120, rgb(24, 22, 40)) cadre(60, 60, 200, 120, wl)
    txt(2, 160, 68, "ARRIVEE", wl)
    txt(5, 160, 90, fmt(T) + " s", blanc)
    txt(1, 160, 134, (T <= best[tk] ? "NOUVEAU RECORD !" : "record " + fmt(best[tk]) + " s") + string.format("   %d chocs", hits), wl)
    txt(1, 160, 156, "<  " + TR[nxt][0] + "  >   P3 = jouer", coul(TR[nxt][2]))
  end
end
